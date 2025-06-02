#include "Render.h"
#include <Windows.h>
#include <mmsystem.h> 
#pragma comment(lib, "winmm.lib") 
#include <GL\GL.h>
#include <GL\GLU.h>
#include <iostream>
#include <iomanip>
#include <cmath>
#include <sstream>
#include "GUItextRectangle.h"
#include "MyShaders.h"
#include "Texture.h"
#define M_PI 3.14

#include "ObjLoader.h"


#include "debout.h"



//внутренняя логика "движка"
#include "MyOGL.h"
extern OpenGL gl;
#include "Light.h"
Light light;
#include "Camera.h"
Camera camera;


bool texturing = true;
bool lightning = true;
bool alpha = false;


//переключение режимов освещения, текстурирования, альфаналожения
void switchModes(OpenGL* sender, KeyEventArg arg)
{
	//конвертируем код клавиши в букву
	auto key = LOWORD(MapVirtualKeyA(arg.key, MAPVK_VK_TO_CHAR));

	switch (key)
	{
	case 'L':
		lightning = !lightning;
		break;
	case 'T':
		texturing = !texturing;
		break;
	case 'A':
		alpha = !alpha;
		break;
	}
}

//умножение матриц c[M1][N1] = a[M1][N1] * b[M2][N2]
template<typename T, int M1, int N1, int M2, int N2>
void MatrixMultiply(const T* a, const T* b, T* c)
{
	for (int i = 0; i < M1; ++i)
	{
		for (int j = 0; j < N2; ++j)
		{
			c[i * N2 + j] = T(0);
			for (int k = 0; k < N1; ++k)
			{
				c[i * N2 + j] += a[i * N1 + k] * b[k * N2 + j];
			}
		}
	}
}

//Текстовый прямоугольничек в верхнем правом углу.
//OGL не предоставляет возможности для хранения текста
//внутри этого класса создается картинка с текстом (через виндовый GDI),
//в виде текстуры накладывается на прямоугольник и рисуется на экране.
//Это самый простой способ что то написать на экране
//но ооооочень не оптимальный
GuiTextRectangle text;

//айдишник для текстуры
GLuint texId;
//выполняется один раз перед первым рендером

ObjModel f;


Shader cassini_sh;
Shader phong_sh;
Shader vb_sh;
Shader simple_texture_sh;

Texture stankin_tex, vb_tex, monkey_tex, samolet_tex, earth_tex, salut1_tex, salut2_tex;



void initRender()
{

	cassini_sh.VshaderFileName = "shaders/v.vert";
	cassini_sh.FshaderFileName = "shaders/cassini.frag";
	cassini_sh.LoadShaderFromFile();
	cassini_sh.Compile();

	phong_sh.VshaderFileName = "shaders/v.vert";
	phong_sh.FshaderFileName = "shaders/light.frag";
	phong_sh.LoadShaderFromFile();
	phong_sh.Compile();

	vb_sh.VshaderFileName = "shaders/v.vert";
	vb_sh.FshaderFileName = "shaders/vb.frag";
	vb_sh.LoadShaderFromFile();
	vb_sh.Compile();

	simple_texture_sh.VshaderFileName = "shaders/v.vert";
	simple_texture_sh.FshaderFileName = "shaders/textureShader.frag";
	simple_texture_sh.LoadShaderFromFile();
	simple_texture_sh.Compile();

	stankin_tex.LoadTexture("textures/stankin.png");
	samolet_tex.LoadTexture("textures/newsam.png");
	earth_tex.LoadTexture("textures/earth.jpg");

	salut1_tex.LoadTexture("textures/salut1.jpg");
	salut2_tex.LoadTexture("textures/salut2.jpg");
	f.LoadModel("models//samolet.obj");
	//==============НАСТРОЙКА ТЕКСТУР================
	//4 байта на хранение пикселя
	glPixelStorei(GL_UNPACK_ALIGNMENT, 4);



	//================НАСТРОЙКА КАМЕРЫ======================
	camera.caclulateCameraPos();

	//привязываем камеру к событиям "движка"
	gl.WheelEvent.reaction(&camera, &Camera::Zoom);
	gl.MouseMovieEvent.reaction(&camera, &Camera::MouseMovie);
	gl.MouseLeaveEvent.reaction(&camera, &Camera::MouseLeave);
	gl.MouseLdownEvent.reaction(&camera, &Camera::MouseStartDrag);
	gl.MouseLupEvent.reaction(&camera, &Camera::MouseStopDrag);
	//==============НАСТРОЙКА СВЕТА===========================
	//привязываем свет к событиям "движка"
	gl.MouseMovieEvent.reaction(&light, &Light::MoveLight);
	gl.KeyDownEvent.reaction(&light, &Light::StartDrug);
	gl.KeyUpEvent.reaction(&light, &Light::StopDrug);
	//========================================================
	//====================Прочее==============================
	gl.KeyDownEvent.reaction(switchModes);
	text.setSize(512, 180);
	//========================================================


	camera.setPosition(2, 1.5, 1.5);

}
float view_matrix[16];
double full_time = 0;
int location = 0;

// Функция для расчета позиции по кривой Безье
std::tuple<double, double, double> bezierCurve(double t, std::tuple<double, double, double> p0, std::tuple<double, double, double> p1, std::tuple<double, double, double> p2, std::tuple<double, double, double> p3) {
	double B_x = pow(1 - t, 3) * std::get<0>(p0) + 3 * t * pow(1 - t, 2) * std::get<0>(p1) + 3 * t * t * (1 - t) * std::get<0>(p2) + pow(t, 3) * std::get<0>(p3);
	double B_y = pow(1 - t, 3) * std::get<1>(p0) + 3 * t * pow(1 - t, 2) * std::get<1>(p1) + 3 * t * t * (1 - t) * std::get<1>(p2) + pow(t, 3) * std::get<1>(p3);
	double B_z = pow(1 - t, 3) * std::get<2>(p0) + 3 * t * pow(1 - t, 2) * std::get<2>(p1) + 3 * t * t * (1 - t) * std::get<2>(p2) + pow(t, 3) * std::get<2>(p3);
	return std::make_tuple(B_x, B_y, B_z);

}

void DrawCube() {
	glBegin(GL_QUADS);

	// Передняя
	glNormal3f(0, 0, 1);
	glVertex3f(-0.5f, -0.5f, 0.5f);
	glVertex3f(0.5f, -0.5f, 0.5f);
	glVertex3f(0.5f, 0.5f, 0.5f);
	glVertex3f(-0.5f, 0.5f, 0.5f);
	// Задняя
	glNormal3f(0, 0, -1);
	glVertex3f(-0.5f, -0.5f, -0.5f);
	glVertex3f(-0.5f, 0.5f, -0.5f);
	glVertex3f(0.5f, 0.5f, -0.5f);
	glVertex3f(0.5f, -0.5f, -0.5f);

	glNormal3f(-1, 0, 0);
	glVertex3f(-0.5f, -0.5f, -0.5f);
	glVertex3f(-0.5f, -0.5f, 0.5f);
	glVertex3f(-0.5f, 0.5f, 0.5f);
	glVertex3f(-0.5f, 0.5f, -0.5f);

	glNormal3f(1, 0, 0);
	glVertex3f(0.5f, -0.5f, -0.5f);
	glVertex3f(0.5f, 0.5f, -0.5f);
	glVertex3f(0.5f, 0.5f, 0.5f);
	glVertex3f(0.5f, -0.5f, 0.5f);

	glNormal3f(0, 1, 0);
	glVertex3f(-0.5f, 0.5f, -0.5f);
	glVertex3f(-0.5f, 0.5f, 0.5f);
	glVertex3f(0.5f, 0.5f, 0.5f);
	glVertex3f(0.5f, 0.5f, -0.5f);

	glNormal3f(0, -1, 0);
	glVertex3f(-0.5f, -0.5f, -0.5f);
	glVertex3f(0.5f, -0.5f, -0.5f);
	glVertex3f(0.5f, -0.5f, 0.5f);
	glVertex3f(-0.5f, -0.5f, 0.5f);
	glEnd();

}

void DrawBrick(float x, float y, float z, float w, float h, float d) {
	glPushMatrix();
	glTranslatef(x, y, z);
	glScalef(w, h, d);
	DrawCube();
	glPopMatrix();
}

void DrawTorus(float x, float y, float z, float outer, float inner,
	float r, float g, float b, int sides, int rings) {
	glPushMatrix();
	glTranslatef(x, y, z);
	glColor3f(r, g, b);

	for (int i = 0; i < sides; i++) {
		float theta1 = 2.0f * M_PI * i / sides;
		float theta2 = 2.0f * M_PI * (i + 1) / sides;

		glBegin(GL_QUAD_STRIP);
		for (int j = 0; j <= rings; j++) {
			float phi = 2.0f * M_PI * j / rings;
			float x1 = outer * cos(theta1) + inner * cos(theta1) * cos(phi);
			float y1 = outer * sin(theta1) + inner * sin(theta1) * cos(phi);
			float z1 = inner * sin(phi);

			float x2 = outer * cos(theta2) + inner * cos(theta2) * cos(phi);
			float y2 = outer * sin(theta2) + inner * sin(theta2) * cos(phi);
			float z2 = inner * sin(phi);

			glNormal3f(cos(theta1) * cos(phi), sin(theta1) * cos(phi), sin(phi));
			glVertex3f(x1, y1, z1);
			glNormal3f(cos(theta2) * cos(phi), sin(theta2) * cos(phi), sin(phi));
			glVertex3f(x2, y2, z2);
		}
		glEnd();
	}
	glPopMatrix();
}

void DrawArchWindow(float width, float height) {
	glBegin(GL_POLYGON);
	glVertex3f(-width / 2, -height / 2, 0);
	glVertex3f(width / 2, -height / 2, 0);

	// Арочная часть
	for (int i = 0; i <= 10; i++) {
		float angle = M_PI * i / 10;
		glVertex3f(width / 2 * cos(angle), height / 2 * sin(angle) - height / 2, 0);
	}
	glEnd();
}


void DrawSphere(float radius, int slices, int stacks) {
	for (int i = 0; i < stacks; i++) {
		float phi1 = M_PI * i / stacks;
		float phi2 = M_PI * (i + 1) / stacks;

		glBegin(GL_QUAD_STRIP);
		for (int j = 0; j <= slices; j++) {
			float theta = 2 * M_PI * j / slices;

			for (int k = 0; k <= 1; k++) {
				float phi = (k == 0) ? phi1 : phi2;
				float x = radius * sin(phi) * cos(theta);
				float y = radius * sin(phi) * sin(theta);
				float z = radius * cos(phi);

				glNormal3f(x / radius, y / radius, z / radius);
				glVertex3f(x, y, z);
			}
		}
		glEnd();
	}
}

void DrawCone(float base, float height, int slices) {
	// Основание 
	glBegin(GL_POLYGON);
	glNormal3f(0, 0, -1);
	for (int i = 0; i <= slices; i++) {
		float angle = 2 * M_PI * i / slices;
		glVertex3f(base * cos(angle), base * sin(angle), 0);
	}

	glEnd();

	// Боковая поверхность
	glBegin(GL_TRIANGLE_FAN);
	glNormal3f(0, 0, 1);
	glVertex3f(0, 0, height);
	for (int i = 0; i <= slices; i++) {
		float angle = 2 * M_PI * i / slices;
		float x = base * cos(angle);
		float y = base * sin(angle);
		glNormal3f(x / height, y / height, base / height);
		glVertex3f(x, y, 0);
	}

	glEnd();
}

void DrawStar(float x, float y, float z, float size, float r, float g, float b) {
	glPushMatrix();
	glTranslatef(x, y, z);
	glColor3f(r, g, b);

	// Центральная часть
	DrawSphere(size * 0.2f, 10, 10);

	// Главные лучи
	for (int i = 0; i < 5; i++) {
		glPushMatrix();
		glRotatef(72 * i, 0, 1, 0);
		glRotatef(45, 1, 0, 0);

		// Главный луч
		glPushMatrix();
		glScalef(size * 0.1f, size * 0.1f, size);
		DrawCube();
		glPopMatrix();

		// Боковые лучи
		glPushMatrix();
		glRotatef(30, 0, 1, 0);
		glScalef(size * 0.06f, size * 0.06f, size * 0.7f);
		DrawCube();
		glPopMatrix();

		glPushMatrix();
		glRotatef(-30, 0, 1, 0);
		glScalef(size * 0.06f, size * 0.06f, size * 0.7f);
		DrawCube();
		glPopMatrix();

		glPopMatrix();
	}
	glPopMatrix();
}
void DrawClock(float x, float y, float z, float radius) {
	glPushMatrix();
	glTranslatef(x, y, z);

	glDisable(GL_LIGHTING);


	glColor3f(1.0f, 1.0f, 0.95f); 
	DrawSphere(radius, 24, 24);

	// Темная окантовка
	glColor3f(0.2f, 0.2f, 0.2f);
	DrawTorus(0, 0, 0, radius, radius * 0.05f, 0.2f, 0.2f, 0.2f, 20, 20);


	glColor3f(0.0f, 0.0f, 0.0f);
	glPushMatrix();
	glRotatef(-90, 0, 0, 1); 


	glPushMatrix();
	glRotatef(30, 0, 0, 1);
	glScalef(radius * 0.05f, radius * 0.6f, radius * 0.02f);
	DrawCube();
	glPopMatrix();

	
	glPushMatrix();
	glRotatef(180, 0, 0, 1); 
	glScalef(radius * 0.03f, radius * 0.9f, radius * 0.02f);
	DrawCube();
	glPopMatrix();

	glPushMatrix();
	glColor3f(0.5f, 0.5f, 0.5f);
	DrawSphere(radius * 0.05f, 10, 10);
	glPopMatrix();

	glPopMatrix();
	glPopMatrix();
}

void DrawSpasskayaTower() {
	glEnable(GL_LIGHTING);
	glEnable(GL_LIGHT0);
	glEnable(GL_COLOR_MATERIAL);
	glColorMaterial(GL_FRONT, GL_AMBIENT_AND_DIFFUSE);

	// Фундамент
	glPushMatrix();
	glTranslatef(0, 0.5f, 0);
	glScalef(1.8f, 1.0f, 1.8f);
	glColor3f(0.5f, 0.3f, 0.2f);
	DrawCube();
	glPopMatrix();


	// Левая стена 
	glPushMatrix();
	glTranslatef(-8.0f, 1.5f, 0); 
	glScalef(14.0f, 4.0f, 1.8f);  
	glColor3f(0.7f, 0.1f, 0.1f);
	DrawCube();


	float brickWidth = 0.18f;
	float brickHeight = 0.08f;
	float mortar = 0.02f; 

	int bricksPerRow = floor(1.0f / (brickWidth + mortar));
	int rows = floor(1.0f / (brickHeight + mortar));

	float xOffset = (1.0f - (bricksPerRow * (brickWidth + mortar) - mortar)) / 2.0f;
	float yOffset = (1.0f - (rows * (brickHeight + mortar) - mortar)) / 2.0f;

	for (int row = 0; row < rows; row++) {
		for (int col = 0; col < bricksPerRow; col++) {
			float x = -0.4f + xOffset + col * (brickWidth + mortar);
			float y = -0.5f + yOffset + row * (brickHeight + mortar);

			glPushMatrix();
			glTranslatef(x, y, 0.51f);
			glScalef(brickWidth, brickHeight, 0.02f);
			glColor3f(0.6f, 0.1f, 0.1f);
			DrawCube();
			glPopMatrix();
		}
	}
	glPopMatrix();

	// Правая стена 
	glPushMatrix();
	glTranslatef(3.0f, 1.5f, 0); 
	glScalef(4.0f, 4.0f, 1.8f); 
	glColor3f(0.7f, 0.1f, 0.1f);
	DrawCube();


	for (float y = -0.48f; y < 0.48f; y += 0.1f) {
		for (float x = -0.4f; x < 0.45f; x += 0.2f) {
			glPushMatrix();
			glTranslatef(x, y, 0.51f);
			glScalef(0.18f, 0.08f, 0.02f);
			glColor3f(0.6f, 0.1f, 0.1f);
			DrawCube();
			glPopMatrix();
		}
	}
	glPopMatrix();

	// Нижний ярус башни 
	glPushMatrix();
	glTranslatef(0, 2.55f, 0);
	glScalef(1.5f, 6.0f, 1.5f);
	glColor3f(0.7f, 0.1f, 0.1f);
	DrawCube();

	for (float y = -0.4f; y < 0.4f; y += 0.1f) {
		for (float x = -0.45f; x < 0.45f; x += 0.1f) {
			glPushMatrix();
			glTranslatef(x, y, 0.51f);
			glScalef(0.18f, 0.08f, 0.02f);
			glColor3f(0.6f, 0.1f, 0.1f);
			DrawCube();
			glPopMatrix();
		}
	}
	glPopMatrix();

	// Средний ярус с окнами
	glPushMatrix();
	glTranslatef(0, 6.3f, 0);
	glScalef(1.2f, 3.5f, 1.2f);
	glColor3f(0.7f, 0.1f, 0.1f);
	DrawCube();

	// Окна 
	for (int i = 0; i < 4; i++) {
		glPushMatrix();
		glRotatef(90 * i, 0, 1, 0);
		glTranslatef(0.51f, 0, 0);

		glPushMatrix();
		glColor3f(0.1f, 0.1f, 0.1f);
		glScalef(0.02f, 0.4f, 0.4f);
		DrawCube();
		glPopMatrix();

		glPushMatrix();
		glTranslatef(0.01f, 0, 0);
		glDisable(GL_LIGHTING);
		glColor3f(0.8f, 0.9f, 1.0f);
		DrawArchWindow(0.35f, 0.6f);
		glEnable(GL_LIGHTING);
		glPopMatrix();

		glPopMatrix();
	}
	glPopMatrix();

	// Часы 
	DrawClock(0, 6.85f, 0.76f, 0.45f);

	// Верхний ярус с зубцами
	glPushMatrix();
	glTranslatef(0, 8.2f, 0);
	glScalef(1.3f, 0.8f, 1.3f);
	glColor3f(0.7f, 0.1f, 0.1f);
	DrawCube();

	// Зубцы
	glColor3f(0.9f, 0.8f, 0.1f);
	for (int i = 0; i < 8; i++) {
		glPushMatrix();
		float angle = 2 * M_PI * i / 8;
		glTranslatef(0.5f * cos(angle), 0.5f, 0.5f * sin(angle));
		glScalef(0.15f, 0.3f, 0.15f);
		DrawCube();
		glPopMatrix();
	}
	glPopMatrix();

	// Шпиль
	glPushMatrix();
	glTranslatef(0, 8.6f, 0);
	glColor3f(0.9f, 0.8f, 0.1f);

	// Основание шпиля
	DrawSphere(0.35f, 16, 16);

	// Конус шпиля 
	glRotatef(-90, 1, 0, 0);
	DrawCone(0.25f, 1.8f, 20);
	glPopMatrix();

	// Кремлевская звезда
	glPushMatrix();
	glTranslatef(0, 10.0f, 0);
	glColor3f(1.0f, 0.9f, 0.1f);
	DrawStar(0, 0, 0, 0.35f, 1.0f, 0.9f, 0.1f);
	glPopMatrix();

	glDisable(GL_LIGHTING);
}

void DrawTree(float x, float y, float z, float trunkHeight, float trunkWidth, float foliageSize) {
	glPushMatrix();
	glTranslatef(x, y, z);

	glColor3f(0.5f, 0.3f, 0.1f);
	glPushMatrix();
	glScalef(trunkWidth, trunkHeight, trunkWidth);
	DrawCube();
	glPopMatrix();

	glColor3f(0.1f, 0.5f, 0.1f);


	glPushMatrix();
	glTranslatef(0, trunkHeight, 0);
	glScalef(foliageSize, foliageSize * 0.7f, foliageSize);
	DrawSphere(1.0f, 10, 10);
	glPopMatrix();

	glPushMatrix();
	glTranslatef(0, trunkHeight + foliageSize * 0.5f, 0);
	glScalef(foliageSize * 0.8f, foliageSize * 0.6f, foliageSize * 0.8f);
	DrawSphere(1.0f, 10, 10);
	glPopMatrix();

	glPushMatrix();
	glTranslatef(0, trunkHeight + foliageSize * 0.9f, 0);
	glScalef(foliageSize * 0.6f, foliageSize * 0.5f, foliageSize * 0.6f);
	DrawSphere(1.0f, 10, 10);
	glPopMatrix();

	glPopMatrix();
}


float time_since_last_chime = 0.0f;
const float chime_interval = 45.0f; 
bool play_chime = false;


void Render(double delta_time)
{
	full_time += delta_time;

	if (gl.isKeyPressed('F'))
	{
		light.SetPosition(camera.x(), camera.y(), camera.z());
	}
	camera.SetUpCamera();
	glGetFloatv(GL_MODELVIEW_MATRIX, view_matrix);
	light.SetUpLight();
	gl.DrawAxes();
	glBindTexture(GL_TEXTURE_2D, 0);
	glEnable(GL_NORMALIZE);
	glDisable(GL_LIGHTING);
	glDisable(GL_TEXTURE_2D);
	glDisable(GL_BLEND);

	if (lightning) glEnable(GL_LIGHTING);
	if (texturing)
	{
		glEnable(GL_TEXTURE_2D);
		glBindTexture(GL_TEXTURE_2D, 0);
	}
	if (alpha)
	{
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	}

	float  amb[] = { 0.2, 0.2, 0.1, 1. };
	float dif[] = { 0.4, 0.65, 0.5, 1. };
	float spec[] = { 0.9, 0.8, 0.3, 1. };
	float sh = 0.2f * 256;
	glMaterialfv(GL_FRONT, GL_AMBIENT, amb);
	glMaterialfv(GL_FRONT, GL_DIFFUSE, dif);
	glMaterialfv(GL_FRONT, GL_SPECULAR, spec);
	glMaterialf(GL_FRONT, GL_SHININESS, sh);
	glShadeModel(GL_SMOOTH);


	time_since_last_chime += delta_time;
	if (time_since_last_chime >= chime_interval)
	{
		time_since_last_chime = 0.0f;
		play_chime = true;
	}

	if (play_chime)
	{
		PlaySound(TEXT("chimes.wav"), NULL, SND_FILENAME | SND_ASYNC);
		play_chime = false;
	}

	
	simple_texture_sh.UseShader();
	location = glGetUniformLocationARB(simple_texture_sh.program, "tex");
	glUniform1iARB(location, 0);
	glActiveTexture(GL_TEXTURE0);
	earth_tex.Bind();

	glPushMatrix();
	glTranslatef(5.0f, 5.0f, -1.0f);  
	glScalef(20.0f, 20.0f, 0.1f);  
	glBegin(GL_QUADS);
	glNormal3f(0, 0, 1);
	glTexCoord2f(1, 1); glVertex3f(0.5f, 0.5f, 0);
	glTexCoord2f(1, 0); glVertex3f(0.5f, -0.5f, 0);
	glTexCoord2f(0, 0); glVertex3f(-0.5f, -0.5f, 0);
	glTexCoord2f(0, 1); glVertex3f(-0.5f, 0.5f, 0);
	glEnd();
	glPopMatrix();



	Shader::DontUseShaders();
	glPushMatrix();
	glTranslatef(-2.0f, 10.0f, -0.5f);  
	glRotatef(90.0f, 1.0f, 0.0f, 0.0f);
	glRotatef(90.0f, 0.0f, 1.0f, 0.0f);
	DrawSpasskayaTower();
	glPopMatrix();

	glPushMatrix();
	glTranslatef(-2.0f, 10.0f, -0.5f); 
	glRotatef(90.0f, 1.0f, 0.0f, 0.0f);
	glRotatef(90.0f, 0.0f, 1.0f, 0.0f);


	DrawTree(-3.0f, 0.0f, 1.5f, 1.2f, 0.2f, 0.8f);  
	DrawTree(3.0f, 0.0f, 1.5f, 1.5f, 0.25f, 1.0f); 

	DrawTree(-4.0f, 0.0f, 2.5f, 1.0f, 0.15f, 0.7f);  
	DrawTree(4.0f, 0.0f, 2.5f, 1.3f, 0.2f, 0.9f);   

	DrawTree(-5.0f, 0.0f, 3.5f, 1.4f, 0.25f, 1.1f);  
	DrawTree(5.0f, 0.0f, 3.5f, 1.1f, 0.2f, 0.8f);   


	glPopMatrix();

	
	vb_sh.UseShader();

	glActiveTexture(GL_TEXTURE0);
	salut1_tex.Bind();
	glActiveTexture(GL_TEXTURE1);
	salut2_tex.Bind();

	location = glGetUniformLocationARB(vb_sh.program, "time");
	glUniform1fARB(location, full_time);
	location = glGetUniformLocationARB(vb_sh.program, "tex_stankin");
	glUniform1iARB(location, 0);
	location = glGetUniformLocationARB(vb_sh.program, "tex_vb");
	glUniform1iARB(location, 1);

	glPushMatrix();
	
	glTranslatef(-5.0f, 5.0f, 9.0f);  
	glRotatef(90.0f, 1.0f, 0.0f, 0.0f);  
	glRotatef(90.0f, 0.0f, 1.0f, 0.0f);

	
	glScalef(20.0f, 20.0f, 0.1f); 
	glBegin(GL_QUADS);
	glNormal3f(0, 0, 1);
	glTexCoord2f(1, 1); glVertex3f(0.5f, 0.5f, 0);
	glTexCoord2f(1, 0); glVertex3f(0.5f, -0.5f, 0);
	glTexCoord2f(0, 0); glVertex3f(-0.5f, -0.5f, 0);
	glTexCoord2f(0, 1); glVertex3f(-0.5f, 0.5f, 0);
	glEnd();
	glPopMatrix();


	std::tuple<double, double, double> p0(1, 0, 16),  
		p1(2, 8, 15),   
		p2(6, 12, 13),
		p3(8, 20, 12);    

	float t = static_cast<float>(fmod(full_time / 15.0, 1.0)); 
	auto pos = bezierCurve(t, p0, p1, p2, p3);
	float t_next = t + 0.01f;
	if (t_next > 1.0f) t_next -= 1.0f;
	auto pos_next = bezierCurve(t_next, p0, p1, p2, p3);
	float dx = std::get<0>(pos_next) - std::get<0>(pos);
	float dy = std::get<1>(pos_next) - std::get<1>(pos);
	float dz = std::get<2>(pos_next) - std::get<2>(pos);
	float angleY = atan2(dx, dz) * 180.0 / M_PI;
	float angleX = -atan2(dy, sqrt(dx * dx + dz * dz)) * 180.0 / M_PI;

	simple_texture_sh.UseShader();
	location = glGetUniformLocationARB(simple_texture_sh.program, "tex");
	glUniform1iARB(location, 0);
	glActiveTexture(GL_TEXTURE0);
	samolet_tex.Bind();

	glPushMatrix();
	glTranslatef(std::get<0>(pos), std::get<1>(pos), std::get<2>(pos));
	glRotatef(angleY, 0, 1, 0);
	glRotatef(angleX, 1, 0, 0);
	glRotatef(90.0f, 0.0f, 0.0f, 1.0f);
	glScalef(0.3, 0.3, 0.3);  
	f.Draw();
	glPopMatrix();

	


	glLoadIdentity();
	camera.SetUpCamera();
	Shader::DontUseShaders();
	light.DrawLightGizmo();


	glActiveTexture(GL_TEXTURE0);
	glMatrixMode(GL_PROJECTION);
	glPushMatrix();
	glLoadIdentity();
	glOrtho(0, gl.getWidth() - 1, 0, gl.getHeight() - 1, 0, 1);
	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();
	glLoadIdentity();

	std::wstringstream ss;
	ss << std::fixed << std::setprecision(3);
	ss << "T - " << (texturing ? L"[вкл]выкл  " : L" вкл[выкл] ") << L"текстур" << std::endl;
	ss << "L - " << (lightning ? L"[вкл]выкл  " : L" вкл[выкл] ") << L"освещение" << std::endl;
	ss << "A - " << (alpha ? L"[вкл]выкл  " : L" вкл[выкл] ") << L"альфа-наложение" << std::endl;
	ss << L"F - Свет из камеры" << std::endl;
	ss << L"G - двигать свет по горизонтали" << std::endl;
	ss << L"G+ЛКМ двигать свет по вертекали" << std::endl;
	ss << L"Коорд. света: (" << std::setw(7) << light.x() << "," << std::setw(7) << light.y() << "," << std::setw(7) << light.z() << ")" << std::endl;
	ss << L"Коорд. камеры: (" << std::setw(7) << camera.x() << "," << std::setw(7) << camera.y() << "," << std::setw(7) << camera.z() << ")" << std::endl;
	ss << L"Параметры камеры: R=" << std::setw(7) << camera.distance() << ",fi1=" << std::setw(7) << camera.fi1() << ",fi2=" << std::setw(7) << camera.fi2() << std::endl;
	ss << L"delta_time: " << std::setprecision(5) << delta_time << std::endl;
	ss << L"full_time: " << std::setprecision(2) << full_time << std::endl;

	text.setPosition(10, gl.getHeight() - 10 - 180);
	text.setText(ss.str().c_str());
	text.Draw();

	glMatrixMode(GL_PROJECTION);
	glPopMatrix();
	glMatrixMode(GL_MODELVIEW);
	glPopMatrix();
}