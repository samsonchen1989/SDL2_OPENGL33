#include <iostream>

// Include GLEW
#include <GL/glew.h>

#include <assert.h>

#include "Game.h"
#include "InputHandler.h"

using namespace std;

#define GL_CHECK_ERRORS assert(glGetError()==GL_NO_ERROR);

//projection and modelview matrices
glm::mat4  P = glm::mat4(1);
glm::mat4 MV = glm::mat4(1);

const int NUM_X = 40; //total quads on X axis
const int NUM_Z = 40; //total quads on Z axis

const float SIZE_X = 4; //size of plane in world space
const float SIZE_Z = 4;
const float HALF_SIZE_X = SIZE_X/2.0f;
const float HALF_SIZE_Z = SIZE_Z/2.0f;

//ripple displacement speed
const float SPEED = 2;

//ripple mesh vertices and indices
glm::vec3 vertices[(NUM_X+1)*(NUM_Z+1)];
const int TOTAL_INDICES = NUM_X*NUM_Z*2*3;
GLushort indices[TOTAL_INDICES];

//camera transformation variables
int state = 0, oldX=0, oldY=0;
float rX=25, rY=-40, dist = -7;

//current time
float time = 0;

//vertex array and vertex buffer object IDs
GLuint vaoID;
GLuint vboVerticesID;
GLuint vboIndicesID;

Game* Game::s_pInstance = 0;

Game::Game():
m_pWindow(0),
m_bRunning(false),
m_pGameStateMachine(0),
m_playerLives(3),
m_scrollSpeed(0.8f),
m_bLevelComplete(false),
m_bChangingState(false)
{
    m_pShader = new GLSLShader();
    // start at this level
    m_currentLevel = 1;
}

Game::~Game()
{
    clean();
}

GAME_STATUS_TAG Game::init(const char* title, int xpos, int ypos, int width, int height, int flags)
{
    // store the game width and height
    m_gameWidth = width;
    m_gameHeight = height;

    //Initialize SDL
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_HAPTIC | SDL_INIT_TIMER) >= 0) {
        // we must wish our OpenGL Version!!
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
        SDL_GL_SetAttribute(SDL_GL_ACCELERATED_VISUAL, 1);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);

        //if succeeded create our window
        m_pWindow = SDL_CreateWindow(title, xpos, ypos, width, height, SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN);

        //If the window creation succeeded create our render
        if (m_pWindow != 0) {
            m_openglContext = SDL_GL_CreateContext(m_pWindow); 
            if (m_openglContext == 0) { 
                std::cout << "Error while creating OpenGL context: " << SDL_GetError() << endl; 
            }
        } else {
            return GAME_ERROR_WINDOW_INIT_FAIL;
        }

        // Initialize GLEW
        glewExperimental = true; // Needed for core profile, or glew func may crash
        if (glewInit() != GLEW_OK) {
            fprintf(stderr, "Failed to initialize GLEW\n");
            return GAME_ERROR_GLEW_INIT_FAIL;
        }
    } else {
        return GAME_ERROR_SDL_INIT_FAIL; //SDL could not initialize
    }

    m_bRunning = true;

    m_pGameStateMachine = new GameStateMachine();
    //m_pGameStateMachine->changeState(new MainMenuState());

    // Enable depth test
    glEnable(GL_DEPTH_TEST);
    // Accept fragment if it closer to the camera than the former one
    glDepthFunc(GL_LESS);
    //set the polygon mode to render lines, no fill
    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

    //GL_CHECK_ERRORS
    //load shader
    m_pShader->LoadFromFile(GL_VERTEX_SHADER, "shaders/shader.vert");
    m_pShader->LoadFromFile(GL_FRAGMENT_SHADER, "shaders/shader.frag");
    //compile and link shader
    m_pShader->CreateAndLinkProgram();
    m_pShader->Use();
        //add shader attribute and uniforms
        m_pShader->AddAttribute("vVertex");
        m_pShader->AddUniform("MVP");
        m_pShader->AddUniform("time");
    m_pShader->UnUse();
    //GL_CHECK_ERRORS

    P = glm::perspective(45.0f, (GLfloat)width/height, 1.f, 1000.f);

    //setup plane geometry
    int count = 0;
    int i=0, j=0;
    for( j=0;j<=NUM_Z;j++) {
        for( i=0;i<=NUM_X;i++) {
            // i/NUM_X, [0, 1]; (i/NUM_X * 2 - 1), [-1, 1]; final range:[-HALF_SIZE_X, HALF_SIZE_X]
            vertices[count++] = glm::vec3( ((float(i)/(NUM_X-1)) *2-1)* HALF_SIZE_X, 0, ((float(j)/(NUM_Z-1))*2-1)*HALF_SIZE_Z);
        }
    }

    //fill plane indices array
    GLushort* id=&indices[0];
    for (i = 0; i < NUM_Z; i++) {
        for (j = 0; j < NUM_X; j++) {
            int i0 = i * (NUM_X+1) + j;
            int i1 = i0 + 1;
            int i2 = i0 + (NUM_X+1);
            int i3 = i2 + 1;
            if ((j+i)%2) {
                *id++ = i0; *id++ = i2; *id++ = i1;
                *id++ = i1; *id++ = i2; *id++ = i3;
            } else {
                *id++ = i0; *id++ = i2; *id++ = i3;
                *id++ = i0; *id++ = i3; *id++ = i1;
            }
        }
    }

    //GL_CHECK_ERRORS

    //setup triangle vao and vbo stuff
    glGenVertexArrays(1, &vaoID);
    glGenBuffers(1, &vboVerticesID);
    glGenBuffers(1, &vboIndicesID);

    glBindVertexArray(vaoID);
        glBindBuffer(GL_ARRAY_BUFFER, vboVerticesID);
        //pass triangle verteices to buffer object
        glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), &vertices[0], GL_STATIC_DRAW);
        cerr << "size:" << sizeof(vertices) << endl;
        //GL_CHECK_ERRORS
        //enable vertex attribute array for position
        glEnableVertexAttribArray((*m_pShader)["vVertex"]);
        cerr << "va pointer:" << (*m_pShader)["vVertex"] << endl;
        glVertexAttribPointer((*m_pShader)["vVertex"], 3, GL_FLOAT, GL_FALSE,0,0);

        //pass indices to element array buffer
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vboIndicesID);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), &indices[0], GL_STATIC_DRAW);
        //GL_CHECK_ERRORS

    return GAME_INIT_SUCCESS;
}

void Game::render()
{
    time = SDL_GetTicks() / 1000.0f * SPEED;

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    //set teh camera viewing transformation
    //MV = (T * (Rx * Ry))
    glm::mat4 T		= glm::translate(glm::mat4(1.0f),glm::vec3(0.0f, 0.0f, dist));
    glm::mat4 Rx	= glm::rotate(T,  rX, glm::vec3(1.0f, 0.0f, 0.0f));
    glm::mat4 MV	= glm::rotate(Rx, rY, glm::vec3(0.0f, 1.0f, 0.0f));
    glm::mat4 MVP	= P*MV;

    //bind the shader
    m_pShader->Use();
        //pass the shader uniform
        glUniformMatrix4fv((*m_pShader)("MVP"), 1, GL_FALSE, glm::value_ptr(P*MV));
        //cerr << "time:" << time << endl;
        glUniform1f((*m_pShader)("time"), time);
        //drwa triangle
        glDrawElements(GL_TRIANGLES, TOTAL_INDICES, GL_UNSIGNED_SHORT, 0);
    //unbind the shader
    m_pShader->UnUse();

    SDL_GL_SwapWindow(m_pWindow);
}

void Game::clean()
{
    cout << "cleaning game\n";

    TheInputHandler::Instance()->clean();

    m_pGameStateMachine->clean();
    m_pGameStateMachine = 0;

    m_pShader->DeleteShaderProgram();
    glDeleteBuffers(1, &vboVerticesID);
    glDeleteBuffers(1, &vboIndicesID);
    glDeleteVertexArrays(1, &vaoID);

    delete m_pGameStateMachine;
    delete m_pShader;

    SDL_GL_DeleteContext(m_openglContext);
    SDL_DestroyWindow(m_pWindow);
    SDL_Quit();
}

void Game::quit()
{
    m_bRunning = false;
}

void Game::handleEvents()
{
    TheInputHandler::Instance()->update();
}

void Game::update()
{
    //m_pGameStateMachine->update();
}