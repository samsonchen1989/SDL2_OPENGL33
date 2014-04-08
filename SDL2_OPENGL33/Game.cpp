#include <iostream>

// Include GLEW
#include <GL/glew.h>

// Include GLM
#include <glm.hpp>
#include <gtc/matrix_transform.hpp>

#include "Game.h"
#include "InputHandler.h"
#include "opengl/GLSLShader.h"

#define GL_CHECK_ERRORS assert(glGetError()==GL_NO_ERROR);

using namespace std;
using namespace glm;

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

    glClearColor(1.0, 0.0, 0.0, 1.0);
    // Enable depth test
    glEnable(GL_DEPTH_TEST);
    // Accept fragment if it closer to the camera than the former one
    glDepthFunc(GL_LESS); 
    //set the polygon mode to render lines
    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

    return GAME_INIT_SUCCESS;
}

void Game::render()
{
    glClear(GL_COLOR_BUFFER_BIT);
    SDL_GL_SwapWindow(m_pWindow);
}

void Game::clean()
{
    cout << "cleaning game\n";

    TheInputHandler::Instance()->clean();

    m_pGameStateMachine->clean();

    m_pGameStateMachine = 0;
    delete m_pGameStateMachine;

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