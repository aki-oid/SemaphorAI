#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>   // wchar_t, fgetws など
#include <locale.h>  // setlocale
#include <math.h>
#include <GL/glut.h>
#include <GL/gl.h>
#include <GL/glu.h>

#include "header/read_bitmap.h"
#include "header/myShape.h"
#include "header/Hiragana_Flag.h"
#include "header/Robot.h"

//------------------------------↓ソケット通信用↓------------------------------
#include <winsock2.h>
#include <windows.h>
#include <ws2tcpip.h>
#include <process.h> // _beginthreadex
#pragma comment(lib, "ws2_32.lib") // Winsockライブラリのリンク
#pragma warning(disable:4996) // 古い関数の警告を無視

typedef int socklen_t;
#define close closesocket
#define sleep(x) Sleep((x)*1000)
#define usleep(x) Sleep((x)/1000)

#define MAX_BUFFER_SIZE 2048 
int g_main_port = 8000; 
int g_sub_port = 8001;
char g_target_ip[64] = "127.0.0.1";

// ★ 排他制御とスレッド関連変数
CRITICAL_SECTION data_mutex; // Windows用クリティカルセクション
volatile int data_received_flag = 0; // データ受信通知用フラグ

//------------------------------↑ソケット通信用↑------------------------------

extern wchar_t inputString[Input_StrSize]; 
extern unsigned char wordsFlag;
extern unsigned char revolveFlag;
extern int mode;

#define KEY_ESC 27
#define XYZ_NUM 3
#define TEXTURE_NUM 3

// 視点・ロボット制御用変数
int xBegin = 0, yBegin = 0;
int mButton;
int idling = 0;
float distance, twist, elevation, azimuth;
float theta = 15;
float robot_pos[3] = {0.0f, 0.0f, 0.0f};  
float robot_angle = 0.0f; 

// ライト設定
float diffuse[] = { 1.0, 1.0, 1.0, 1.0 };
float specular[] = { 0.2, 0.2, 0.2, 1.0 };
float ambient[] = { 0.2, 0.2, 0.2, 1.0 };
float shininess = 8.0;
float light_position[] = {4.0, 8.0, 15.0, 1.0};

static char draw_str[Input_StrSize], str[Input_StrSize];
GLuint Round_List;

void polarview();
void resetview();
void drawNormal( float*, float* );
void drawString(char *str, float x0,float y0, double w, double h);
void initTexture();
void mySpecialKey(int key, int x, int y);
void mySpecialKeyUp(int key, int x, int y);
void walkAnimation(int value);

void load_env() {
    FILE *file = fopen(".env", "r");
    if (file == NULL) {
        printf("Info: .env file not found. Using system environment or defaults.\n");
        return;
    }

    char line[256];
    while (fgets(line, sizeof(line), file)) {
        // 改行コード削除
        line[strcspn(line, "\n")] = 0;
        
        // コメント(#)や空行はスキップ
        if (line[0] == '#' || strlen(line) == 0) continue;

        // '=' で分割
        char *delimiter = strchr(line, '=');
        if (delimiter != NULL) {
            *delimiter = '\0'; // '=' を終端文字に置き換えてキーにする
            char *key = line;
            char *value = delimiter + 1;
            
            _putenv_s(key, value);

        }
    }
    fclose(file);
}
// ==========================================
//  ソケットサーバー用スレッド関数
// ==========================================
unsigned __stdcall socket_server_thread(void *arg){

    int server_fd, new_socket;
    struct sockaddr_in address;
    int addrlen = sizeof(address);
    char buffer[MAX_BUFFER_SIZE] = {0};
    
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("Socket failed"); return 0;
    }
    
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));
    
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(g_main_port);
    
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("Bind failed"); return 0;
    }
    
    if (listen(server_fd, 5) < 0) {
        perror("Listen failed"); return 0;
    }
    
    printf(">>> Server Listening on port %d <<<\n", g_main_port);
    
    while(1) {
        // 接続待ち
        if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen)) < 0) {
            perror("Accept failed"); continue;
        }
        
        // データ受信ループ
        char received_data[MAX_BUFFER_SIZE] = {0};
        int valread;
        
            while ((valread = recv(new_socket, buffer, MAX_BUFFER_SIZE - 1, 0)) > 0) {
        
            buffer[valread] = '\0';
            strcat(received_data, buffer);
            if (received_data[strlen(received_data) - 1] == '\n') {
                received_data[strlen(received_data) - 1] = '\0';
                break;
            }
        }
        
        if (strlen(received_data) > 0) {
            printf("あなた: %s\n", received_data);
            EnterCriticalSection(&data_mutex);
            
            // UTF-8 -> ワイド文字変換
            mbstowcs(inputString, received_data, Input_StrSize);
            
            // 受信フラグを立てる
            data_received_flag = 1;
            LeaveCriticalSection(&data_mutex);
        }
        
        closesocket(new_socket);
        printf("Client Disconnected.\n");
    }
    
    closesocket(server_fd);

    return 0;
}

void send_done_to_python() {
    int sock;
    struct sockaddr_in server_addr;

    // ソケット作成
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Sync Socket creation error");
        return;
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(g_sub_port);
    server_addr.sin_addr.s_addr = inet_addr(g_target_ip);

    // Pythonサーバー(待機中)に接続
    if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        // Python側が待機していない場合(モード切替時など)は無視してOK
        close(sock);
        return;
    }

    char *msg = "DONE";
    send(sock, msg, strlen(msg), 0);
    
    close(sock);
}

// ==========================================
//  描画　関数
// ==========================================
void display(void)
{
	glClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glPushMatrix ();
		polarview();//カメラビュー
		glLightfv(GL_LIGHT0, GL_POSITION, light_position);
		
		glEnable( GL_DEPTH_TEST );
			glMaterialfv( GL_FRONT_AND_BACK, GL_DIFFUSE, diffuse );//表面属性の設定
			glMaterialfv( GL_FRONT_AND_BACK, GL_SPECULAR, specular );
			glMaterialfv( GL_FRONT_AND_BACK, GL_AMBIENT, ambient );
			//glMaterialf( GL_FRONT, GL_SHININESS, shininess );

			glEnable( GL_LIGHTING );
				glEnable(GL_COLOR_MATERIAL);
					glColorMaterial(GL_FRONT, GL_AMBIENT_AND_DIFFUSE);
					glColor3f(0.8,0.8,0.8);

					
					glPushMatrix ();
						glTranslatef(robot_pos[0], robot_pos[1], robot_pos[2]);
						glRotatef(robot_angle, 0.0f, 1.0f, 0.0f);

		//座標設定//////////////////////////////////////////////////////////////////////////////////////////////////
						for(left_right = RIGHT; left_right <= LEFT; left_right++){
							for(int j = 0;j<2;j++){//足の座標設定
								if((Leg_LR[left_right].xyz[j][X] == 0.0)&&(Leg_LR[left_right].xyz[j][Y] == 0.0)&&(Leg_LR[left_right].xyz[j][Z] == 0.0)){
									for(i_xyz=0;i_xyz<XYZ_NUM;i_xyz++)
										Leg_LR[left_right].xyz[j][i_xyz] = Leg_LR[left_right].xyz_def[j][i_xyz];
								}
							}
							if((Arm_LR[left_right].xyz[0][X] == 0.0)&&(Arm_LR[left_right].xyz[0][Y] == 0.0)&&(Arm_LR[left_right].xyz[0][Z] == 0.0)){
								for(i_xyz=0;i_xyz<XYZ_NUM;i_xyz++)//肩の座標設定
									Arm_LR[left_right].xyz[0][i_xyz] = Arm_LR[left_right].xyz_def[0][i_xyz];
							}

							if(wordsFlag == TRUE){//手首の座標設定
								for(i_xyz=0;i_xyz<XYZ_NUM;i_xyz++)
									Arm_LR[left_right].xyz[1][i_xyz] = get_coordinates_xyz_from_char(left_right, str_num, motion_num, i_xyz);
							}else{
								for(i_xyz=0;i_xyz<XYZ_NUM;i_xyz++)
									Arm_LR[left_right].xyz[1][i_xyz] = Arm_LR[left_right].xyz_def[1][i_xyz];
							}
							flag_Arm(&Arm_LR[left_right]);//旗の座標設定
						}
		//モデリング//////////////////////////////////////////////////////////////////////////////////////////////////
		/////足
						for(left_right = RIGHT; left_right <= LEFT; left_right++){
							glPushMatrix();//脛
								glTranslatef(Leg_LR[left_right].xyz[0][X], Leg_LR[left_right].xyz[0][Y], Leg_LR[left_right].xyz[0][Z]);// 開始点に移動
								rad_length_Leg(&Leg_LR[left_right],0);
								if (Leg_LR[left_right].axis_x[0] != 0.0 || Leg_LR[left_right].axis_y[0] != 0.0 || Leg_LR[left_right].axis_z[0] != 0.0)
									glRotatef(Leg_LR[left_right].angle[0], Leg_LR[left_right].axis_x[0], Leg_LR[left_right].axis_y[0], Leg_LR[left_right].axis_z[0]);

								Leg_LR[left_right].quad[0] = gluNewQuadric();
								gluCylinder(Leg_LR[left_right].quad[0], 0.25, 0.25, Leg_LR[left_right].length[0], 20, 20); // 半径0.1、長さlength

								gluDeleteQuadric(Leg_LR[left_right].quad[0]); // クワドリックオブジェクトを削除
							glPopMatrix();

							glPushMatrix();//足先
								glTranslatef(Leg_LR[left_right].xyz[1][X],Leg_LR[left_right].xyz[1][Y],Leg_LR[left_right].xyz[1][Z]);
								glScalef(0.3, 0.3, 0.3);

								glutSolidSphere( 1.0, 10, 10 );
							glPopMatrix();
						}

		////胴体
						glPushMatrix();
							glTranslatef(0.0,2.25,0.0);
							glScalef(2.0, 1.5, 0.99);
							glutSolidCube( 1.0 );
						glPopMatrix();

		////腕
						for(left_right = RIGHT; left_right <= LEFT; left_right++){
							for(i = 0; i < 1; i++){
								glPushMatrix();//肩から手首まで
									glTranslatef(Arm_LR[left_right].xyz[0][X], Arm_LR[left_right].xyz[0][Y], Arm_LR[left_right].xyz[0][Z]);// 開始点に移動
									rad_length_Arm(&Arm_LR[left_right],0);
									if (Arm_LR[left_right].axis_x[0] != 0.0 || Arm_LR[left_right].axis_y[0] != 0.0 || Arm_LR[left_right].axis_z[0] != 0.0)
										glRotatef(Arm_LR[left_right].angle[0], Arm_LR[left_right].axis_x[0], Arm_LR[left_right].axis_y[0], Arm_LR[left_right].axis_z[0]);

									Arm_LR[left_right].quad[0] = gluNewQuadric();// 円柱を作成
									gluCylinder(Arm_LR[left_right].quad[0], 0.25, 0.25, Arm_LR[left_right].length[0], 20, 20); // 半径0.1、長さlength

									gluDeleteQuadric(Arm_LR[left_right].quad[0]); // クワドリックオブジェクトを削除
								glPopMatrix();
							}
		glDisable( GL_DEPTH_TEST );
							glPushMatrix();//旗
								if(left_right == RIGHT)
									glColor3f(1.0,0,0);
								glBegin(GL_QUADS);
									glVertex3f( Arm_LR[left_right].xyz[1][X], Arm_LR[left_right].xyz[1][Y], Arm_LR[left_right].xyz[1][Z]);
									glVertex3f( Arm_LR[left_right].flag_xyz[0][X], Arm_LR[left_right].flag_xyz[0][Y], Arm_LR[left_right].flag_xyz[0][Z]);
									glVertex3f( Arm_LR[left_right].flag_xyz[1][X], Arm_LR[left_right].flag_xyz[1][Y], Arm_LR[left_right].flag_xyz[0][Z]);
									glVertex3f( Arm_LR[left_right].flag_xyz[2][X], Arm_LR[left_right].flag_xyz[2][Y], Arm_LR[left_right].xyz[1][Z]);
								glEnd();
							glPopMatrix();
		glEnable( GL_DEPTH_TEST );
							glColor3f(0.8,0.8,0.8);
							glPushMatrix();//手先
								glTranslatef(Arm_LR[left_right].xyz[1][X],Arm_LR[left_right].xyz[1][Y],Arm_LR[left_right].xyz[1][Z]);
								glScalef(0.5, 0.5, 0.5);
								glutSolidSphere( 1.0, 10, 10 );
							glPopMatrix();
						}
						
		////顔
						glPushMatrix();//顔
							glTranslatef(0.0,4.0,0.0);
							glScalef(3.0, 2.0, 1.99);
							glutSolidCube( 1.0 );
						glPopMatrix();

						glPushMatrix();//帽子
							glTranslatef(0.0,5.0,0.0);
							glScalef(0.75, 0.75, 0.5);
							glutSolidCube( 1.0 );
						glPopMatrix();

		//テクスチャマッピング/////////////////////////////////////////////////////////////
						glEnable(GL_TEXTURE_2D);
							glBindTexture(GL_TEXTURE_2D, 1);
							glPushMatrix();// 顔の表情
								glTranslatef(0.0, 4.0, 1.0);  
								glBegin(GL_QUADS);
									glNormal3f(0.0, 0.0, 1.0);
									glTexCoord2f(0.0, 1.0); glVertex3f(-1.5, 1.0, 0.0);
									glTexCoord2f(1.0, 1.0); glVertex3f( 1.5, 1.0, 0.0);
									glTexCoord2f(1.0, 0.0); glVertex3f( 1.5,-1.0, 0.0);
									glTexCoord2f(0.0, 0.0); glVertex3f(-1.5,-1.0, 0.0);
								glEnd();
							glPopMatrix();

							glBindTexture(GL_TEXTURE_2D, 2);
							glPushMatrix();// テクスチャ付き前面
								glTranslatef(0.0, 2.25, 0.5);
								glBegin(GL_QUADS);
									glNormal3f(0.0, 0.0, 1.0);
									glTexCoord2f(0.0, 1.0); glVertex3f(-1, 0.75, 0.0);
									glTexCoord2f(1.0, 1.0); glVertex3f( 1, 0.75, 0.0);
									glTexCoord2f(1.0, 0.0); glVertex3f( 1,-0.75, 0.0);
									glTexCoord2f(0.0, 0.0); glVertex3f(-1,-0.75, 0.0);
								glEnd();
							glPopMatrix();
							
							glBindTexture(GL_TEXTURE_2D, 3);
							glPushMatrix();// テクスチャ付き背面
								glTranslatef(0.0, 2.25, -0.5);
								glBegin(GL_QUADS);
									glNormal3f(0.0, 0.0, -1.0);
									glTexCoord2f(0.0, 1.0); glVertex3f(-1, 0.75, 0.0);
									glTexCoord2f(1.0, 1.0); glVertex3f( 1, 0.75, 0.0);
									glTexCoord2f(1.0, 0.0); glVertex3f( 1,-0.75, 0.0);
									glTexCoord2f(0.0, 0.0); glVertex3f(-1,-0.75, 0.0);
								glEnd();
							glPopMatrix();
						glPopMatrix();

					glDisable(GL_TEXTURE_2D);
				glDisable(GL_COLOR_MATERIAL);
			glDisable( GL_LIGHTING );

//その他/////////////////////////////////////////////////////////////
			glCallList(Round_List); // ディスプレイリストとして地面の線を呼び出して描画
			glPushMatrix();//点光源イメージ
				glColor3f(1.0,1.0,0.0);
				glTranslatef(light_position[X], light_position[Y], light_position[Z]);
				glutSolidSphere( 1.0, 10, 10 );
			glPopMatrix();
		glDisable( GL_DEPTH_TEST );
	glPopMatrix ();
    
//文字列表示 (モードに応じて分岐)
    glPushMatrix();
        if(wordsFlag==TRUE)
            drawString(draw_str,50,400,500,500);
        else
            drawString("Robo-ta",175,400,500,500);

        if (mode == MODE_flag) {
			if((revolveFlag==FALSE)&&(wordsFlag==FALSE)){
				glPushMatrix();//文字
					drawString("Waiting for input",25,700,750,750);
				glPopMatrix();
			}else if(revolveFlag==FALSE){
				glPushMatrix();//文字
					drawString("STOP",25,700,750,750);
				glPopMatrix();
			}
        } else if (mode == MODE_walking) {
			glPushMatrix();//文字
				drawString("Walking",25,700,750,750);
			glPopMatrix();
        } else if (mode == MODE_talking) {
			glPushMatrix();//文字
				drawString("Talking",25,700,750,750);
			glPopMatrix();
        } else {
            drawString("Waiting for Python...",25,700,750,750);
        }
    glPopMatrix();
    
    glutSwapBuffers(); 
}

void drawNormal( float *v0, float *v1 )
{
	float x0, y0, z0;

	glColor3f( 1.0, 0.0, 0.0 );
	glLineWidth( 2.0 );
	x0= v0[X], y0= v0[1], z0= v0[2];
	glPushMatrix();
		glTranslatef( x0, y0, z0 );
		glBegin( GL_LINES );
			glVertex3f( 0.0, 0.0, 0.0 );
			glVertex3fv( v1 );
		glEnd();
	glPopMatrix();
}
void drawString(char *str, float x0,float y0, double w, double h) {
    int i,len;
    glMatrixMode(GL_PROJECTION); glPushMatrix();
    glColor3d(1.,0.,0.); glLoadIdentity(); gluOrtho2D(0.,w,0.,h);
    len = strlen(str);
    for(i=0;i<len;i++){
        glRasterPos2f(x0+20*i,y0);
        glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, *str);
        str++;
    }
    glPopMatrix(); glMatrixMode(GL_MODELVIEW);
}

// =============================================================
//  idle関数
//  キーボード入力(scanf)を廃止し、ソケット受信データを監視する
// =============================================================
void idle(void)
{
    // 1. ソケットからのデータ受信をチェック
    EnterCriticalSection(&data_mutex);
    if (data_received_flag == 1) {
        data_received_flag = 0;
        
        // inputString: "1:あいうえお"
        int command_mode = inputString[0] - L'0'; 
        wchar_t *body_ptr = &inputString[2]; 
        if (wcslen(inputString) < 3) body_ptr = L"";

        mode = command_mode; 

        // 安全に文字列をセット (一時バッファ使用)
        wchar_t temp_buffer[Input_StrSize];
        wcscpy(temp_buffer, body_ptr);
        wcscpy(inputString, temp_buffer);

        // 各モードの初期化
        if (mode == MODE_flag) {
            if (wcslen(inputString) > 0 && wcscmp(inputString, L"INIT") != 0) {
                // アニメーション開始トリガー
                wordsFlag = GL_TRUE;
                revolveFlag = GL_TRUE;
                str_num = 0;
                motion_num = 0;
                memset(draw_str, 0, sizeof(draw_str));
                is_walking = FALSE;
                idling = 0;
            }
        }
        else if (mode == MODE_walking) {
            wordsFlag = GL_FALSE;
            is_walking = TRUE;
            revolveFlag = GL_TRUE; 
        }
        else if (mode == MODE_talking) {
            wordsFlag = GL_FALSE; 
            revolveFlag = GL_TRUE; 
        }
        else if(mode == MODE_exit){
            exit(0);
        }
        glutPostRedisplay();
    }
    LeaveCriticalSection(&data_mutex);

    // 2. 手旗信号アニメーション処理
    if(mode == MODE_flag && wordsFlag == TRUE){
        if((motion_num == 0)&&(idling==0)){
            Beep(1320, 100); 
            strcat(draw_str, reverse_roma(str_num));
            wprintf(L" %c ",inputString[str_num]);
        }
        idling++;
        printf("Debug: idling = %d\n", idling);
        if(idling == 30){
            idling = 0;
            motion_num++; 
            if((motion_num >= 5)||(get_coordinates_xyz_from_char(RIGHT, str_num, motion_num, X) == -100)){
                str_num++;
                motion_num = 0;
            }
            if(inputString[str_num] == L'\0'){
                Beep(1320, 400); 
                printf("\nAnimation Finished: %ls\n", inputString);
                
                // アニメーション終了
                wordsFlag = GL_FALSE; 
                
                send_done_to_python();
            }
        }
    }
    
    // 再描画が必要な場合のみコールバック
    if (revolveFlag) {
        glutPostRedisplay();
    }
}

void myKbd( unsigned char key, int x, int y )
{
    if((mode!=MODE_select)&&((key == 'w') ||(key == 's')||(key == 'd')|| (key == 'a'))){
        float step = 0.2f;

        if (key == 'w') {
            robot_pos[2] -= step;
            if (robot_pos[2]<=-RANGE)
                robot_pos[2]=-RANGE;
            robot_angle = 180.0f;
        } else if (key == 's') {
            robot_pos[2] += step;
            if (robot_pos[2]>=RANGE)
                robot_pos[2]=RANGE;
            robot_angle = 0.0f;
        } else if (key == 'd') {
            robot_pos[0] += step;
            if (robot_pos[0]>=RANGE)
                robot_pos[0]=RANGE;
            robot_angle = 90.0f;
        } else if (key == 'a') {
            robot_pos[0] -= step;
            if (robot_pos[0]<=-RANGE)
                robot_pos[0]=-RANGE;
            robot_angle = -90.0f;
        }
        is_walking = TRUE;
        walk_frame = 0;
        glutTimerFunc(0, walkAnimation, 0);
        glutPostRedisplay();
    }

    switch( key ) {
        case 'R':
            is_walking = FALSE;
            resetview();
            glutPostRedisplay();
            break;
        case 'c':
            if(mode==MODE_walking)
                mode = MODE_select;
            break;
        case ' ':
            revolveFlag = !revolveFlag;
            if(revolveFlag == GL_TRUE)
                glutIdleFunc(idle); //idleを繰り返し実行するように設定
            else
                glutIdleFunc(NULL);//アニメーションをとめる
            break;
        case KEY_ESC:
            exit( 0 );
    }
}
void myKeyboardUp(unsigned char key, int x, int y) {
    switch (key) {
        case 'w':
        case 'a':
        case 's':
        case 'd':
            is_walking = FALSE;
            // 足を元の位置に戻す
            for (int lr = RIGHT; lr <= LEFT; lr++) {
                for (int i = 0; i < XYZ_NUM; i++) {
                    Leg_LR[lr].xyz[1][i] = Leg_LR[lr].xyz_def[1][i];
                }
            }
            glutPostRedisplay();
            break;
    }
}

void myMouse(int button, int state, int x, int y)
{
    if (((mode==MODE_walking)||(wordsFlag == GL_TRUE))&&(state == GLUT_DOWN)) {
        xBegin = x;
        yBegin = y;
        mButton = button;
    }
}

void myMotion(int x, int y)
{
    if((mode==MODE_walking)||(wordsFlag == GL_TRUE)){
        int xDisp, yDisp;
        xDisp = x - xBegin;
        yDisp = y - yBegin;
        switch(mButton){
        case GLUT_LEFT_BUTTON:
            azimuth += (double) xDisp/2.0;
            elevation -= (double) yDisp/2.0;
            break;
        case GLUT_MIDDLE_BUTTON:
            twist = fmod (twist + xDisp, 360.0);
            break;
        case GLUT_RIGHT_BUTTON:
            distance -= (double) yDisp/40.0;
            twist += xDisp/2.0;
            break;
        }
        xBegin = x;
        yBegin = y;
        glutPostRedisplay();
    }
}

void initTexture(void)
{
    struct {
        const char *filename;
        unsigned char *image;
        int width, height;
    }
    textures[TEXTURE_NUM] = {
        {"bmp/face.bmp", NULL, 0, 0},
        {"bmp/body1.bmp", NULL, 0, 0},
        {"bmp/body2.bmp", NULL, 0, 0}
    };

    for (int i = 0; i < TEXTURE_NUM; i++) {
        if (!ReadBitMapData((char *)textures[i].filename, &textures[i].width, &textures[i].height, &textures[i].image)) {
            // テクスチャファイルが見つからない場合に安全に終了し、クラッシュを防ぐ
            fprintf(stderr, "FATAL ERROR: BMP file not found or failed to load: %s\n", textures[i].filename);
            exit(1); 
        }

        glBindTexture(GL_TEXTURE_2D, i + 1); // テクスチャIDは1から
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexImage2D(GL_TEXTURE_2D, 0, 4, textures[i].width, textures[i].height, 0,
                     GL_RGBA, GL_UNSIGNED_BYTE, textures[i].image);
        glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
    }
}

void myInit (char *progname)
{
    glutInitWindowPosition(0, 0);
    glutInitWindowSize( 500, 450);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGBA | GLUT_DEPTH);
    glutCreateWindow(progname);
    glClearColor (0.0, 0.25, 0.75, 1.0);//背景色
    glutKeyboardFunc( myKbd );
    glutKeyboardUpFunc(myKeyboardUp);//通常キーを離したとき
    glutMouseFunc( myMouse );
    glutMotionFunc( myMotion );
    resetview();
    
    initTexture();
    glShadeModel( GL_SMOOTH );
    glEnable( GL_LIGHT0 );
    Round_List = createRound();
    {//各関節座標の初期化
        Leg_LR[RIGHT].xyz_def[0][X] = -0.75;//脛
        Leg_LR[RIGHT].xyz_def[0][Y] = 1.5;
        Leg_LR[RIGHT].xyz_def[0][Z] = 0.0;
        Leg_LR[RIGHT].xyz_def[1][X] = -1.0;//足先
        Leg_LR[RIGHT].xyz_def[1][Y] = 0.5;
        Leg_LR[RIGHT].xyz_def[1][Z] = 0.0;

        Arm_LR[RIGHT].xyz_def[0][X] = -1.0;//肩
        Arm_LR[RIGHT].xyz_def[0][Y] = 2.5;
        Arm_LR[RIGHT].xyz_def[0][Z] = 0.25;

        for(i_xyz = 0; i_xyz < XYZ_NUM; i_xyz++){
            Leg_LR[LEFT].xyz_def[0][i_xyz] = Leg_LR[RIGHT].xyz_def[0][i_xyz];
            Leg_LR[LEFT].xyz_def[1][i_xyz] = Leg_LR[RIGHT].xyz_def[1][i_xyz];
            Arm_LR[LEFT].xyz_def[0][i_xyz] = Arm_LR[RIGHT].xyz_def[0][i_xyz];
            if(i_xyz == X){
                Leg_LR[LEFT].xyz_def[0][i_xyz] *= -1.0;//脛
                Leg_LR[LEFT].xyz_def[1][i_xyz] *= -1.0;//足先
                Arm_LR[LEFT].xyz_def[0][i_xyz] *= -1.0;//肩
            }

            Arm_LR[LEFT].xyz_def[1][i_xyz] = flag_locate[1][LEFT][i_xyz];//手首
            Arm_LR[RIGHT].xyz_def[1][i_xyz] = flag_locate[7][RIGHT][i_xyz];
        }
    }
}

void myReshape(int width, int height)
{
    float aspect = (float) width / (float) height;

    glViewport(0, 0, width, height);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(60.0, aspect, 1.0, 100.0);
    glMatrixMode(GL_MODELVIEW);
}

void polarview( void )
{
    glTranslatef( -robot_pos[0]*2/3, 0.0, -distance);
    glRotatef( -twist, 0.0, 0.0, 1.0);
    glRotatef( -elevation, 1.0, 0.0, 0.0);
    glRotatef( -azimuth, 0.0, 1.0, 0.0);
}

void resetview( void )
{
    distance = RANGE;
    twist = 0.0;
    elevation = -15.0;
    azimuth = 0.0;

    robot_angle = 0.0f;         // Z軸負方向を向く
    robot_pos[0] = 0;         // Xマイナス方向へ移動
    robot_pos[2] = 0;         // Zプラス方向へ後退
}

int main(int argc, char** argv)
{
    load_env();
    setlocale(LC_ALL, ""); 

    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        printf("WSAStartup failed.\n");
        return 1;
    }
    // Mutex初期化
    InitializeCriticalSection(&data_mutex);

    char* ip_env     = getenv("HOST");
    char* port_str_1 = getenv("MAIN_PORT");
    char* port_str_2 = getenv("SUB_PORT");

    // 1. 環境変数取得
    if (ip_env != NULL) {
        strncpy(g_target_ip, ip_env, sizeof(g_target_ip) - 1);
        g_target_ip[sizeof(g_target_ip) - 1] = '\0';
    } else {
        printf("Warning: HOST env not found. Using default: %s\n", g_target_ip);
    }

    if (port_str_1 != NULL) {
        g_main_port = atoi(port_str_1);
    } else {
        printf("Warning: MAIN_PORT not found. Using default: %d\n", g_main_port);
    }
    if (port_str_2 != NULL) {
        g_sub_port = atoi(port_str_2);
    } else {
        printf("Warning: SUB_PORT not found. Using default: %d\n", g_sub_port);
    }

    printf("Settings -> Main Port: %d\n, Sub Port: %d\n, Target IP: %s\n\n", 
           g_main_port, g_sub_port, g_target_ip);

    // 2. スレッド起動
    HANDLE hThread = (HANDLE)_beginthreadex(NULL, 0, socket_server_thread, NULL, 0, NULL);
    if (hThread == 0) {
        perror("Could not create server thread");
        return 1;
    }

    // 3. GLUT初期化とループ開始
    glutInit(&argc, argv);
    myInit(argv[0]);
    glutReshapeFunc(myReshape);
    glutDisplayFunc(display);
    glutIdleFunc(idle);
    glutMainLoop(); 

    // 通常ここには到達しない
    WSACleanup();
    DeleteCriticalSection(&data_mutex);
    return 0;
}