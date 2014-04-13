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

//ripple mesh vertices and indices
//mesh vertices and indices
glm::vec2 vertices[4];
GLushort indices[6];

//texture image filename
const string filename = "media/Lenna.png";

//vertex array and vertex buffer object IDs
GLuint vaoID;
GLuint vboVerticesID;
GLuint vboIndicesID;

//texture ID
GLuint textureID;

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

    //GL_CHECK_ERRORS
    //load shader
    m_pShader->LoadFromFile(GL_VERTEX_SHADER, "shaders/shader.vert");
    m_pShader->LoadFromFile(GL_FRAGMENT_SHADER, "shaders/shader.frag");
    //compile and link shader
    m_pShader->CreateAndLinkProgram();
    m_pShader->Use();
        //add shader attribute and uniforms
        m_pShader->AddAttribute("vVertex");
        m_pShader->AddUniform("textureMap");
        //pass values of constant uniforms at initialization
        glUniform1i((*m_pShader)("textureMap"), 0);
        
    m_pShader->UnUse();
    //GL_CHECK_ERRORS

    //setup quad geometry
    //setup quad vertices
    vertices[0] = glm::vec2(0.0,0.0);
    vertices[1] = glm::vec2(1.0,0.0);
    vertices[2] = glm::vec2(1.0,1.0);
    vertices[3] = glm::vec2(0.0,1.0);

    //fill quad indices array
    GLushort* id=&indices[0];
    *id++ =0;
    *id++ =1;
    *id++ =2;
    *id++ =0;
    *id++ =2;
    *id++ =3;

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
        glVertexAttribPointer((*m_pShader)["vVertex"], 2, GL_FLOAT, GL_FALSE,0,0);

        //pass indices to element array buffer
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vboIndicesID);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), &indices[0], GL_STATIC_DRAW);
        //GL_CHECK_ERRORS

        	//load the image using SOIL
	int texture_width = 0, texture_height = 0, channels=0;
	GLubyte* pData = SOIL_load_image(filename.c_str(), &texture_width, &texture_height, &channels, SOIL_LOAD_AUTO);
	if(pData == NULL) {
		cerr<<"Cannot load image: "<<filename.c_str()<<endl;
		exit(EXIT_FAILURE);
	}
	//vertically flip the image on Y axis since it is inverted
	int i,j;
	for( j = 0; j*2 < texture_height; ++j )
	{
		int index1 = j * texture_width * channels;
		int index2 = (texture_height - 1 - j) * texture_width * channels;
		for( i = texture_width * channels; i > 0; --i )
		{
			GLubyte temp = pData[index1];
			pData[index1] = pData[index2];
			pData[index2] = temp;
			++index1;
			++index2;
		}
	}
	//setup OpenGL texture and bind to texture unit 0
	glGenTextures(1, &textureID);
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, textureID);
		//set texture parameters
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);

		//allocate texture 
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, texture_width, texture_height, 0, GL_RGB, GL_UNSIGNED_BYTE, pData);


    return GAME_INIT_SUCCESS;
}

void Game::render()
{
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    //bind shader
    m_pShader->Use();
        //draw the full screen quad
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, 0);
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