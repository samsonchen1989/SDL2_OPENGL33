#include <iostream>

// Include GLEW
#include <GL/glew.h>

#include <assert.h>

#include "Game.h"
#include "InputHandler.h"
#include "opengl/FreeCamera.h"

using namespace std;

#define GL_CHECK_ERRORS assert(glGetError()==GL_NO_ERROR);

Game* Game::s_pInstance = 0;

//projection and modelview matrices
glm::mat4  P = glm::mat4(1);
glm::mat4 MV = glm::mat4(1);

//camera transformation variables
int state = 0, oldX=0, oldY=0;
float rX=20, rY=64, dist = -7;

//skybox object
#include "opengl/skybox.h"
CSkybox* skybox;
//skybox texture ID
GLuint skyboxTextureID;

#include "opengl/WaterSurface.h"
CWaterSurface* water;

//skybox texture names
const char* texture_names[6] = {
    "media/skybox/ocean/posx.png",
    "media/skybox/ocean/negx.png",
    "media/skybox/ocean/posy.png",
    "media/skybox/ocean/negy.png",
    "media/skybox/ocean/posz.png",
    "media/skybox/ocean/negz.png"};

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

void handleInput()
{
    int x = TheInputHandler::Instance()->getMousePosition()->getX();
    int y = TheInputHandler::Instance()->getMousePosition()->getY();

    rY += (x - oldX)/5.0f;
    rX += (y - oldY)/5.0f;

    oldX = x;
    oldY = y;
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
    //glEnable(GL_CULL_FACE);
    // Accept fragment if it closer to the camera than the former one
    //glDepthFunc(GL_LESS);

    //generate a new Skybox
    skybox = new CSkybox();

    water = new CWaterSurface(1000,1000,1000,1000);

    int texture_widths[6];
    int texture_heights[6];
    int channels[6];
    GLubyte* pData[6];

    cout << "Loading skybox images: ..." << endl;
    for (int i = 0; i < 6; i++) {
        cout << "\tLoading: "<< texture_names[i] << "...";
        pData[i] = SOIL_load_image(texture_names[i], &texture_widths[i], &texture_heights[i], &channels[i], SOIL_LOAD_AUTO);
        cout << "done." << endl;
    }

    //generate OpenGL texture
    glGenTextures(1, &skyboxTextureID);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_CUBE_MAP, skyboxTextureID);

    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    //glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    // glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

    GLint format = (channels[0] == 4) ? GL_RGBA : GL_RGB;
    //load the 6 image
    for (int i = 0; i < 6; i++) {
        //allocate cubemap data
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, format, texture_widths[i], texture_heights[i], 0, format, GL_UNSIGNED_BYTE, pData[i]);
        SOIL_free_image_data(pData[i]);
    }

    //setup the projection matrix
    P = glm::perspective(60.0f, (GLfloat)width/height, 0.1f, 1000.f);

    SDL_WarpMouseInWindow(m_pWindow, width / 2, height / 2);
    oldX = width / 2;
    oldY = height / 2;

    cout<<"Initialization successfull"<<endl;

    return GAME_INIT_SUCCESS;
}

void Game::render()
{
    float time = SDL_GetTicks() / 1000.0f * 0.1f;
    //clear color and depth buffers
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    //set the camera transform
    glm::mat4 T = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, dist));
    glm::mat4 Rx = glm::rotate(glm::mat4(1), rX, glm::vec3(1.0f, 0.0f, 0.0f));
    glm::mat4 MV = glm::rotate(Rx, rY, glm::vec3(0.0f, 1.0f, 0.0f));
    glm::mat4 S = glm::scale(glm::mat4(1), glm::vec3(1000.0));
    glm::mat4 MVP = P * MV * S;

    //render the skybox object
    skybox->Render( glm::value_ptr(MVP));

    MV = T*MV; 
    water->SetTime(time); 

    glm::vec3 eyePos;
    eyePos.x = -(MV[0][0] * MV[3][0] + MV[0][1] * MV[3][1] + MV[0][2] * MV[3][2]);
    eyePos.y = -(MV[1][0] * MV[3][0] + MV[1][1] * MV[3][1] + MV[1][2] * MV[3][2]);
    eyePos.z = -(MV[2][0] * MV[3][0] + MV[2][1] * MV[3][1] + MV[2][2] * MV[3][2]);
    water->SetEyePos(eyePos);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    water->Render(glm::value_ptr(P*MV));
    glDisable(GL_BLEND);

    SDL_GL_SwapWindow(m_pWindow);
}

void Game::clean()
{
    cout << "cleaning game\n";

    TheInputHandler::Instance()->clean();

    m_pGameStateMachine->clean();
    m_pGameStateMachine = 0;

    m_pShader->DeleteShaderProgram();

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
    handleInput();
}

void Game::update()
{
    //m_pGameStateMachine->update();
}