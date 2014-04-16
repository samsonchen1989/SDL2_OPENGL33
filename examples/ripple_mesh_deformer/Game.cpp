#include <iostream>

// Include GLEW
#include <GL/glew.h>

#include <assert.h>

#include "Game.h"
#include "InputHandler.h"
#include "opengl/FreeCamera.h"

using namespace std;

#define GL_CHECK_ERRORS assert(glGetError()==GL_NO_ERROR);

//shader reference
GLSLShader shader;

//vertex array and vertex buffer object IDs
GLuint vaoID;
GLuint vboVerticesID;
GLuint vboIndicesID;

const int NUM_X = 40; //total quads on X axis
const int NUM_Z = 40; //total quads on Z axis

const float SIZE_X = 4; //size of plane in world space
const float SIZE_Z = 4;
const float HALF_SIZE_X = SIZE_X/2.0f;
const float HALF_SIZE_Z = SIZE_Z/2.0f;

//ripple displacement speed
const float SPEED = 2;

//for floating point imprecision
const float EPSILON = 0.001f;
const float EPSILON2 = EPSILON*EPSILON;

//ripple mesh vertices and indices
glm::vec3 vertices[(NUM_X+1)*(NUM_Z+1)];
const int TOTAL_INDICES = NUM_X*NUM_Z*2*3;
GLushort indices[TOTAL_INDICES];

//projection and modelview matrices
glm::mat4  P = glm::mat4(1);
glm::mat4 MV = glm::mat4(1);

//camera tranformation variables
int state = 1, oldX=0, oldY=0;
float rX=0, rY=0, fov = 45;

//current time
float time = 0;

//delta time
float dt = 0;

//timing related variables
float last_time=0, current_time =0;

//free camera instance
CFreeCamera cam;

int count = 0;

//mouse filtering support variables
const float MOUSE_FILTER_WEIGHT=0.75f;
const int MOUSE_HISTORY_BUFFER_SIZE = 10;

//mouse history buffer
glm::vec2 mouseHistory[MOUSE_HISTORY_BUFFER_SIZE];

float mouseX=0, mouseY=0; //filtered mouse values

//flag to enable filtering
bool useFiltering = true;

Game* Game::s_pInstance = 0;

//mouse move filtering function
void filterMouseMoves(float dx, float dy) {
    for (int i = MOUSE_HISTORY_BUFFER_SIZE - 1; i > 0; --i) {
        mouseHistory[i] = mouseHistory[i - 1];
    }

    // Store current mouse entry at front of array.
    mouseHistory[0] = glm::vec2(dx, dy);

    float averageX = 0.0f;
    float averageY = 0.0f;
    float averageTotal = 0.0f;
    float currentWeight = 1.0f;

    // Filter the mouse.
    for (int i = 0; i < MOUSE_HISTORY_BUFFER_SIZE; ++i)
    {
        glm::vec2 tmp=mouseHistory[i];
        averageX += tmp.x * currentWeight;
        averageY += tmp.y * currentWeight;
        averageTotal += 1.0f * currentWeight;
        currentWeight *= MOUSE_FILTER_WEIGHT;
    }

    mouseX = averageX / averageTotal;
    mouseY = averageY / averageTotal;
}

void handleInput()
{
    //handle keyboard input
    if (TheInputHandler::Instance()->isKeyDown(SDL_SCANCODE_W)) {
        cam.Walk(dt);
    }

    if (TheInputHandler::Instance()->isKeyDown(SDL_SCANCODE_S)) {
        cam.Walk(-dt);
    }

    if (TheInputHandler::Instance()->isKeyDown(SDL_SCANCODE_A)) {
        cam.Strafe(-dt);
    }

    if (TheInputHandler::Instance()->isKeyDown(SDL_SCANCODE_D)) {
        cam.Strafe(dt);
    }

    if (TheInputHandler::Instance()->isKeyDown(SDL_SCANCODE_Q)) {
        cam.Lift(dt);
    }

    if (TheInputHandler::Instance()->isKeyDown(SDL_SCANCODE_V)) {
        cam.Lift(-dt);
    }

    glm::vec3 t = cam.GetTranslation(); 
    if(glm::dot(t,t)>EPSILON2) {
        cam.SetTranslation(t*0.95f);
    }

    //handle mouse position change
    Vector2D *mouseVector = TheInputHandler::Instance()->getMousePosition();
    int x = mouseVector->getX();
    int y = mouseVector->getY();

    if (x == 0 && y == 0) {
        return;
    }
    
    if (state == 0) {
        fov += (y - oldY)/5.0f;
        cam.SetupProjection(fov, cam.GetAspectRatio());
    } else {
        rY += (y - oldY)/10.0f;
        rX += (oldX - x)/10.0f;
        if(useFiltering)
            filterMouseMoves(rX, rY);
        else { 
            mouseX = rX;
            mouseY = rY;
        }
        cam.Rotate(mouseX, mouseY, 0);
    }

    oldX = x;
    oldY = y;
}

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

    SDL_WarpMouseInWindow(m_pWindow, width / 2, height / 2);
    oldX = width / 2;
    oldY = height / 2;

    m_bRunning = true;

    m_pGameStateMachine = new GameStateMachine();
    //m_pGameStateMachine->changeState(new MainMenuState());

    // Enable depth test
    glEnable(GL_DEPTH_TEST);
    // Accept fragment if it closer to the camera than the former one
    glDepthFunc(GL_LESS);

    //set the polygon mode to render lines
    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

    //GL_CHECK_ERRORS
    //load shader
    shader.LoadFromFile(GL_VERTEX_SHADER, "shaders/shader.vert");
    shader.LoadFromFile(GL_FRAGMENT_SHADER, "shaders/shader.frag");
    //compile and link shader
    shader.CreateAndLinkProgram();
    shader.Use();
    //add shader attribute and uniforms
    shader.AddAttribute("vVertex");
    shader.AddUniform("MVP");
    shader.AddUniform("time");
    shader.UnUse();

    //GL_CHECK_ERRORS

    //setup plane geometry
    //setup plane vertices
    int count = 0;
    int i=0, j=0;
    for( j=0;j<=NUM_Z;j++) {
        for( i=0;i<=NUM_X;i++) {
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

    //setup plane vao and vbo stuff
    glGenVertexArrays(1, &vaoID);
    glGenBuffers(1, &vboVerticesID);
    glGenBuffers(1, &vboIndicesID);

    glBindVertexArray(vaoID);

    glBindBuffer (GL_ARRAY_BUFFER, vboVerticesID);
    //pass plane vertices to array buffer object
    glBufferData (GL_ARRAY_BUFFER, sizeof(vertices), &vertices[0], GL_STATIC_DRAW);
    //GL_CHECK_ERRORS
    //enable vertex attrib array for position
    glEnableVertexAttribArray(shader["vVertex"]);
    glVertexAttribPointer(shader["vVertex"], 3, GL_FLOAT, GL_FALSE,0,0);
    //GL_CHECK_ERRORS
    //pass the plane indices to element array buffer
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vboIndicesID);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), &indices[0], GL_STATIC_DRAW);
    //GL_CHECK_ERRORS

    //setup camera
    //setup the camera position and look direction
    glm::vec3 p = glm::vec3(5);
    cam.SetPosition(p);
    glm::vec3 look =  glm::normalize(p);

    //rotate the camera for proper orientation
    float yaw = glm::degrees(float(atan2(look.z, look.x)+M_PI));
    float pitch = glm::degrees(asin(look.y));
    rX = yaw;
    rY = pitch;
    if(useFiltering) {
        for (int i = 0; i < MOUSE_HISTORY_BUFFER_SIZE ; ++i) {
            mouseHistory[i] = glm::vec2(rX, rY);
        }
    }

    cam.Rotate(rX,rY,0);
    cam.SetupProjection(45, (GLfloat)width/height);

    cout<<"Initialization successfull"<<endl;

    return GAME_INIT_SUCCESS;
}

void Game::render()
{
    last_time = current_time;
    current_time = SDL_GetTicks() / 1000.0f;
    dt = current_time - last_time;

    //clear color buffer and depth buffer
    glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);

    //set the camera transformation
    glm::mat4 MV	= cam.GetViewMatrix();
    glm::mat4 P     = cam.GetProjectionMatrix();
    glm::mat4 MVP	= P*MV;

    //bind the shader 
    shader.Use();
    //set the shader uniforms
    glUniformMatrix4fv(shader("MVP"), 1, GL_FALSE, glm::value_ptr(MVP));
    glUniform1f(shader("time"), current_time * 2);
    //draw the mesh triangles
    glDrawElements(GL_TRIANGLES, TOTAL_INDICES, GL_UNSIGNED_SHORT, 0);

    //unbind the shader
    shader.UnUse();

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