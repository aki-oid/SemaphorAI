/*  myShape.h   Copyright (c) 2003 by T. HAYASHI and K. KATO  */
/*                                       All rights reserved  */
#include <math.h>
#define PI2 2.0*3.1415926534

void myCircle( float r, int n )
{
    float x, y, z, dq;
	int i;

    dq = PI2/(float)n;
	glPushMatrix();
	y = 0.0;
    glBegin( GL_LINE_LOOP );
	for( i=0; i< n; i++ ){
		x = r*cos( dq*(float)i );
		z = r*sin( dq*(float)i );
		glVertex3f( x, y, z);
	}
	glEnd();
	glPopMatrix();
}


void myDisc( float r, int n )
{
    float x, y, z, dq;
	int i;

    glEnable( GL_NORMALIZE );
    dq = PI2/(float)n;
	glPushMatrix();
	y = 0.0;
    glBegin( GL_POLYGON );
	glNormal3f( 0.0, 1.0, 0.0 );
    for( i=0; i<n; i+=1 ){
		x = r*cos( dq*(float)i );
		z = r*sin( dq*(float)i );
		glVertex3f( x, y, z );
    }
    glEnd();
	glPopMatrix();
    glDisable( GL_NORMALIZE );
}


void mySolidCylinder( float r, float h, int n )
{
    float x, y, z, dq;
	int i;

    glEnable( GL_NORMALIZE );
    dq = PI2/(float)n;
	y = 0.5*h;
	glPushMatrix();
	glRotatef( -dq*180.0/PI2, 0.0, 0.1, 0.0 );
    glBegin( GL_QUAD_STRIP );
    for( i=0; i<=n; i+=1 ){
		x = r*cos( dq*(float)i );
		z = r*sin( dq*(float)i );
		glNormal3f( x,  0, z );
		glVertex3f( x,  y, z );
		glVertex3f( x, -y, z );
    }
    glEnd();
    glBegin( GL_POLYGON );
	glNormal3f( 0.0, -1.0, 0.0 );
   	for( i=0; i<n; i+=1 ){
		x = r*cos( dq*(float)i );
		z = r*sin( dq*(float)i );
		glVertex3f( x, -y, z );
    }
    glEnd();
    glBegin( GL_POLYGON );
	glNormal3f( 0.0, 1.0, 0.0 );
    for( i=0; i<n; i+=1 ){
		x = r*cos( dq*(float)i );
		z = r*sin( dq*(float)i );
		glVertex3f( x, y, z );
    }
    glEnd();
	glPopMatrix();
    glDisable( GL_NORMALIZE );
}


void myWireCylinder( float r, float h, int n )
{
    float x, y, z, dq;
	int i;

    dq = PI2/(float)n;
	y = 0.5*h;
	glPushMatrix();
	glRotatef( -dq*180.0/PI2, 0.0, 1.0, 0.0 );
    glBegin( GL_LINES );
    for( i=0; i< n; i++ ){
	x = r*cos( dq*(float)i );
	z = r*sin( dq*(float)i );
		glVertex3f( x,  y, z);
		glVertex3f( x, -y, z );
	}
    glEnd();

    glBegin( GL_LINE_LOOP );
	for( i=0; i< n; i++ ){
		x = r*cos( dq*(float)i );
		z = r*sin( dq*(float)i );
		glVertex3f( x, y, z);
	}
	glEnd();

    glBegin( GL_LINE_LOOP );
	for( i=0; i< n; i++ ){
		x = r*cos( dq*(float)i );
		z = r*sin( dq*(float)i );
		glVertex3f( x, -y, z);
	}
	glEnd();
	glPopMatrix();
}

