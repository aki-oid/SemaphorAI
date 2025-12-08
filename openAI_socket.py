import socket
import sys
import os
from dotenv import load_dotenv
from playsound import playsound
from openai import OpenAI

import unicodedata  # 追加: 全角→半角変換用
import speech_recognition as sr  # 追加: 音声認識用
from gtts import gTTS #追加: 音声合成用ライブラリ

load_dotenv()
# --- ソケット設定 ---
HOST = os.getenv("HOST")  # ラズパイ内部のローカルホストアドレス
MAIN_PORT = int(os.getenv("MAIN_PORT"))        # C言語サーバーが待ち受けるポート番号
SUB_PORT = int(os.getenv("SUB_PORT"))   # C言語からの完了通知を受け取るポート
# --------------------

client = OpenAI()

class Mode:
    FLAG    = "1"      # 手旗信号モード
    TALKING = "2"      # お話モード
    WALKING = "3"      # 散策（走り込み）モード
    EXIT    = "exit"   # 終了
    
    CMD_QUIT = "9"     # C言語終了用のシグナル番号（9番を終了合図とする）

def recognize_speech():
    """マイクから音声を拾ってテキストに変換する"""
    r = sr.Recognizer()
    
    # マイクの確認
    try:
        mic = sr.Microphone()
    except OSError:
        print("\n[ERROR] マイクが見つかりません。")
        return None

    with mic as source:
        print("\n(音声を待機中... 話しかけてください)")
        # 周囲の雑音レベルを調整
        r.adjust_for_ambient_noise(source)
        try:
            # 録音開始（5秒間無音ならタイムアウト）
            audio = r.listen(source, timeout=5.0, phrase_time_limit=10.0)
            print("(認識中...)")
            # Googleの音声認識サーバーを使用（日本語）
            text = r.recognize_google(audio, language='ja-JP')
            print(f"音声認識結果: {text}")
            return text
        except sr.WaitTimeoutError:
            print("[INFO] 音声が検出されませんでした。")
            return None
        except sr.UnknownValueError:
            print("[INFO] 言葉を聞き取れませんでした。")
            return None
        except sr.RequestError:
            print("[ERROR] 音声認識サービスに接続できません。ネットワークを確認してください。")
            return None
        except Exception as e:
            print(f"[ERROR] 音声認識エラー: {e}")
            return None

def speak_text(text):
    """テキストを音声に変換して再生する"""
    if not text:
        return
    
    try:
        speech_file_path = "temp_speech.mp3"
        # OpenAIの音声合成APIを使用
        with client.audio.speech.with_streaming_response.create(
            model="tts-1",
            voice="onyx",
            input=text,
            speed=1.2
        ) as response:
            # ストリーミングとしてファイルに保存
            response.stream_to_file(speech_file_path)
        
        # 再生 (mpg321を使用)
        playsound("speech_file_path")
    except Exception as e:
        print(f"[ERROR] 音声再生エラー: {e}")

# ------------------------
def normalize_mode_input(text):
    """
    音声や全角入力を半角数字のモードIDに変換する
    例: "１"->"1", "いち"->"1", "手旗"->"1", "終了"->"exit"
    """
    if not text:
        return ""
    
    # 1. 全角英数字を半角に変換 (例: "１" -> "1")
    text = unicodedata.normalize('NFKC', text)
    text = text.strip()

    # 2. キーワードマッピング (音声で入力されそうな言葉)
    # モード1
    if text in ["1", "一", "いち", "イチ", "手旗", "手旗信号", "ワン", "one"]:
        return Mode.FLAG
    # モード2
    if text in ["2", "二", "に", "ニ", "対話", "相談", "ツー", "two"]:
        return Mode.TALKING
    # モード3
    if text in ["3", "三", "さん", "サン", "走り込み", "走る","スリー" , "three"]:
        return Mode.WALKING
    # 終了
    if text in ["exit", "終了", "終わり", "ストップ", "バイバイ"]:
        return Mode.EXIT

    return text

def send_to_c_server(text, mode):
    """C言語サーバーにデータを送信する"""
    if not text and mode not in [Mode.WALKING, Mode.CMD_QUIT]:
        return

    data_to_send = f"{mode}:{text}".strip() + "\n"

    try:
        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
            s.connect((HOST, MAIN_PORT))
            s.sendall(data_to_send.encode('cp932', errors='replace'))
    except Exception as e:
        print(f"\n[ERROR] Cサーバー通信エラー: {e}", file=sys.stderr)

def wait_for_c_animation():
    """C言語側のアニメーション終了通知を待つ"""
    try:
        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
            s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
            s.bind((HOST, SUB_PORT))
            s.listen(1)
            s.settimeout(60.0) # アニメーションが長い場合に備えて長めに
            
            conn, addr = s.accept()
            with conn:
                conn.recv(1024)
    except socket.timeout:
        print("\n[WARN] アニメーション同期タイムアウト")
    except Exception as e:
        print(f"\n[ERROR] 同期エラー: {e}")

def get_system_prompt(mode):
    if mode == Mode.FLAG:
        return {
            "role": "system",
            "content": "あなたは海兵学校の手旗信号教官「ロボタ」です。ユーザーの入力を全て「ひらがな」に変換してください。挨拶不要。"
                "変換ルール１、小文字の「や,ゆ,よ,つ,あ行」などは大文字に変換してください。"
                "変換ルール２、記号は取り除いてください。"
                "変換ルール３、入力されたカタカナ漢字は全てひらがなに変換してください。"
                "必ずひらがなで出力します"
        }
    elif mode == Mode.TALKING:
        return {
            "role": "system",
            "content": "あなたは熱血海兵学校の手旗信号担当教官「ロボタ」です。"
                "口調はですますではなく、「～だ！」「～しろ！」などと断定的に話します。"
                "ユーザーは新人訓練生です。"
                "応答は長すぎず、手旗信号の重要性を説いたり、精神論を交えつつ、ユーザーの質問に答えてください。"
        }
    return None

def session_loop(mode):
    """個別のモード内での対話ループ"""
    
    system_prompt = get_system_prompt(mode)
    conversation_history = [system_prompt] if system_prompt else []

    print(f"\n--- モード{mode} 開始 (終了するには 'quit' と入力) ---")
    print("※ テキスト入力、または何も入力せずEnterを押すと音声入力になります。")

    send_to_c_server("INIT", mode)

    while True:
        try:
            user_input = input("\nあなた (Enterで音声入力) > ").strip()
        except KeyboardInterrupt:
            print("\n強制終了")
            break

        # 音声入力処理
        if not user_input:
            voice_text = recognize_speech()
            if voice_text:
                user_input = voice_text
            else:
                continue # 音声認識失敗時はループの先頭に戻る

        # Quit check
        if user_input in ["終了", "quit", "exit"]:
            print(f"--- モード{mode} 終了 ---\n")
            break

        conversation_history.append({"role": "user", "content": user_input})

        # AI応答生成
        try:
            response = client.chat.completions.create(
                model="gpt-4o-mini",
                messages=conversation_history,
                stream=True,
            )
        except Exception as e:
            print(f"API Error: {e}")
            break

        print("教官:", end=" ", flush=True)
        full_response = ""
        for chunk in response:
            content = chunk.choices[0].delta.content
            if content:
                print(content, end="", flush=True)
                full_response += content
        
        send_to_c_server(full_response, mode)
        conversation_history.append({"role": "assistant", "content": full_response})
        
        speak_text(full_response)
        if mode == Mode.FLAG:
            wait_for_c_animation()

def main():
    """メインメニューのループ"""
    print("--- ラズパイ手旗信号システム Pythonクライアント ---\n")
    print("教官：よく来た新人！")
    print("教官：ここは海兵手旗信号講座の会場である！！")
    print("教官：ここに来てくれたこと心嬉しく思う！！\n")
    print("教官：新人には3つ選択肢がある！！！さぁ選べ！！")

    while True:
        print("-----------------------------------------")
        print(f"{Mode.FLAG}: 手旗信号講座 (AI翻訳 -> アニメーション)")
        print(f"{Mode.TALKING}: 対話・相談 (AI会話)")
        print(f"{Mode.WALKING}: 散策・走り込み (WASD操作)")
        print(f"{Mode.EXIT}: プログラム終了")
        print("※ 数字を入力、またはEnterキーで音声選択が可能")
        
        try:
            raw_input = input("モードを選択 > ").strip()
        except KeyboardInterrupt:
            break

        # 音声入力処理
        mode_text = raw_input
        if not mode_text: # Enterのみの場合
            voice_text = recognize_speech()
            if voice_text:
                mode_text = voice_text
            else:
                continue
        
        mode = normalize_mode_input(mode_text)

        if mode == Mode.EXIT:
            print("教官：以上で本講義を終わりにする！解散！")
            send_to_c_server("EXIT", Mode.CMD_QUIT)
            break
            
        elif mode == Mode.WALKING:
            send_to_c_server("", Mode.WALKING)
            print("\n--- 走り込みモード ---")
            print("C言語ウィンドウで操作してください。")
            print("戻るには Enter キーを押してください...")
            input() 
            
        elif mode in [Mode.FLAG, Mode.TALKING]:
            session_loop(mode)
            
        else:
            print("無効な入力です。")

if __name__ == "__main__":
    main()