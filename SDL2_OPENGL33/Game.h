#ifndef GAME_H
#define GAME_H

#include <vector>
#include <SDL.h>

// Include GLM
#include <glm.hpp>
#include <gtc/matrix_transform.hpp>
#include <gtc/type_ptr.hpp>

#include "Log.h"
#include "GameObject.h"
#include "GameStateMachine.h"
#include "opengl/GLSLShader.h"

//out vertex struct for interleaved attributes
struct Vertex {
    glm::vec3 position;
    glm::vec3 color;
};

class Game
{
public:
    static Game* Instance()
    {
        if (s_pInstance == 0) {
            s_pInstance = new Game();
            return s_pInstance;
        }

        return s_pInstance;
    }

    void render();
    void update();
    void handleEvents();
    void quit();
    void clean();

    GAME_STATUS_TAG init(const char* title, int xpos, int ypos,
        int width, int height, int flags);

    bool running() { return m_bRunning; }

    GameStateMachine* getStateMachine() { return m_pGameStateMachine; }

    int getGameWidth() const { return m_gameWidth; }
    int getGameHeight() const { return m_gameHeight; }
    float getScrollSpeed() { return m_scrollSpeed; }

    void setPlayerLives(int lives) { m_playerLives = lives; }
    int getPlayerLives() { return m_playerLives; }

    void setCurrentLevel(int currentLevel);
    const int getCurrentLevel() { return m_currentLevel; }

    void setLevelComplete(bool levelComplete) { m_bLevelComplete = levelComplete; }
    const bool getLevelComplete() { return m_bLevelComplete; }

    std::vector<std::string> getLevelFiles() { return m_levelFiles; }

private:
    Game();
    virtual ~Game();
    static Game* s_pInstance;

    bool m_bRunning;
    SDL_Window* m_pWindow;
    SDL_GLContext m_openglContext;
    GLSLShader* m_pShader;

    int m_gameWidth;
    int m_gameHeight;
    float m_scrollSpeed;

    int m_playerLives;

    int m_currentLevel;

    bool m_bLevelComplete;

    bool m_bChangingState;

    std::vector<GameObject*> m_gameObjects;
    GameStateMachine* m_pGameStateMachine;

    std::vector<std::string> m_levelFiles;
};

typedef Game TheGame;

#endif