// PES1PG24CS036_VR_Assignment.cpp : Rocket Simulation with Staging and Orbital Physics 
// This simulation models a multi-stage rocket launch to deploy a satellite into Earth orbit.
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include <glut.h> // Required for OpenGL Utility Toolkit (GLUT) functions for windowing and interaction.
#include <cmath> // For mathematical functions like sqrt, sin, cos, acos.
#include <cstdlib> // For general utilities like rand, srand.
#include <ctime> // For time-related functions, used to seed the random number generator.
#include <string> // For using std::string for mission status text.
#include <vector> // For using std::vector to store stars.
#include <iostream> // For input/output operations (though not extensively used in this visual simulation).

// --- Structs and Constants ---
#define PI 3.1415926535 // Define PI for mathematical calculations.

// Structure to represent a 3D vector (position or velocity).
struct Vector3D {
    float x, y, z;
};

// Structure to represent a physical object in the simulation.
struct PhysicsObject {
    Vector3D position; // Current position of the object in 3D space.
    Vector3D velocity; // Current velocity of the object in 3D space.
    float angle;       // Angle of the object (used for rotation, e.g., for booster tumble).
    bool isVisible;    // Flag to control object rendering.
    bool hasThrust;    // Flag to indicate if the object is currently thrusting.
};

// Enumeration for the different states of the rocket launch mission.
enum GameState {
    PRE_LAUNCH,           // Before launch, awaiting user input.
    LIFTOFF,              // Main booster is firing and rocket is ascending.
    STAGE_SEPARATION,     // Main booster separates, upper stage ignites.
    ORBITAL_INSERTION,    // Upper stage performs burn to reach orbital altitude and velocity.
    SATELLITE_DEPLOYMENT, // Satellite is released from the upper stage.
    MISSION_SUCCESS       // Satellite is in a stable orbit.
};

GameState currentState = PRE_LAUNCH; // Initialize the game state to PRE_LAUNCH.
std::string missionStatusText;       // Text displayed on screen to indicate mission status.

// Physics Constants
const float BOOSTER_THRUST = 23.0f;     // Thrust force applied by the main booster.
const float UPPER_STAGE_THRUST = 18.0f; // Thrust force applied by the upper stage.
const float TIMESTEP = 0.016f;          // Simulation time step in seconds (approximately 60 FPS).
// MODIFIED: Increased gravity to ensure a stable orbit can be achieved with the calculated velocity.
const float GRAVITATIONAL_CONSTANT = 2500.0f; // Constant for gravitational force calculation.

// Scene Properties
const float WORLD_OFFSET_Y = -100.0f; // Original was 15.0f. Making it much lower.
const float EARTH_RADIUS = 15.0f;     // Keep Earth radius the same for now, or increase slightly if desired.
const float ORBIT_ALTITUDE = 45.0f;   // This is relative to the Earth's surface (radius + this value).
const int NUM_STARS = 1500;
std::vector<Vector3D> stars;

// Game Objects
PhysicsObject mainBooster;  // Represents the first stage of the rocket.
PhysicsObject upperStage;   // Represents the second stage of the rocket.
PhysicsObject satellite;    // Represents the satellite to be deployed.

GLuint earthTextureID;      // Variable to hold our Earth texture ID
float cameraAngle = 0.0f; // For orbiting camera

// --- Function Declarations ---
void loadTexture();
void resetSimulation();     // Resets all game objects and states to initial conditions.
void setupStars();          // Generates random positions for the background stars.
void drawStars();           // Renders the stars in the scene.
void drawText(const std::string& text, float x, float y); // Renders text on the screen.
void setMaterial(float r, float g, float b, float shine); // Sets OpenGL material properties for drawing.
void drawMainBooster();     // Renders the main booster model.
void drawUpperStage();      // Renders the upper stage model.
void drawSatellite();       // Renders the satellite model.
void drawEarth();           // Renders the Earth model.
void display();             // The main OpenGL display callback function.
void reshape(int w, int h); // The OpenGL reshape callback function (handles window resizing).
void timer(int value);      // The OpenGL timer callback function for animation and physics updates.
void keyboard(unsigned char key, int x, int y); // The OpenGL keyboard callback for user input.
void setupScene();          // Initializes OpenGL settings and game elements.

// --- Drawing Functions ---

// Loads a texture from a file and creates an OpenGL texture object.
void loadTexture() {
    int width, height, nrChannels;
    stbi_set_flip_vertically_on_load(true);
    unsigned char* data = stbi_load("earth_texture.jpg", &width, &height, &nrChannels, 0);

    if (data) {
        glGenTextures(1, &earthTextureID);
        glBindTexture(GL_TEXTURE_2D, earthTextureID);

        // Set texture wrapping and filtering options
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        // Load the texture data
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
        stbi_image_free(data); // Free the image memory
    }
    else {
        std::cout << "Texture failed to load at path: earth_texture.jpg" << std::endl;
        stbi_image_free(data);
    }
}

// Draws the background stars as points.
void drawStars() {
    glDisable(GL_LIGHTING); // Stars are self-illuminating, so disable lighting for them.
    glPointSize(1.5f);      // Set the size of each star point.
    glColor3f(1.0f, 1.0f, 1.0f); // Set star color to white.
    glBegin(GL_POINTS);     // Start drawing points.
    for (const auto& star : stars) { // Iterate through all stored star positions.
        glVertex3f(star.x, star.y, star.z); // Draw a star at its calculated position.
    }
    glEnd();                // End drawing points.
    glEnable(GL_LIGHTING);  // Re-enable lighting for other objects.
}

// Draws 2D text on the screen, typically for status messages.
void drawText(const std::string& text, float x, float y) {
    glMatrixMode(GL_PROJECTION); // Switch to projection matrix to set up 2D orthographic view.
    glPushMatrix();              // Save the current projection matrix.
    glLoadIdentity();            // Reset the projection matrix.
    gluOrtho2D(0, 800, 0, 800);  // Set up a 2D orthographic projection for screen coordinates.
    glMatrixMode(GL_MODELVIEW);  // Switch back to modelview matrix.
    glPushMatrix();              // Save the current modelview matrix.
    glLoadIdentity();            // Reset the modelview matrix.
    glDisable(GL_LIGHTING);      // Disable lighting for text, as it should always be visible.
    glColor3f(1.0f, 1.0f, 1.0f); // Set text color to white.
    glRasterPos2f(x, y);       // Set the raster position for drawing text.
    for (char c : text) {       // Iterate through each character in the text string.
        glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, c); // Draw each character using a bitmap font.
    }
    glEnable(GL_LIGHTING);       // Re-enable lighting.
    glMatrixMode(GL_PROJECTION); // Switch back to projection matrix to restore.
    glPopMatrix();               // Restore the previous projection matrix.
    glMatrixMode(GL_MODELVIEW);  // Switch back to modelview matrix.
    glPopMatrix();               // Restore the previous modelview matrix.
}

// Sets the material properties for subsequent OpenGL drawing commands.
void setMaterial(float r, float g, float b, float shine) {
    glColor3f(r, g, b); // Set the diffuse color of the material.
    GLfloat mat_specular[] = { 0.8f, 0.8f, 0.8f, 1.0f }; // Define specular color.
    GLfloat mat_shininess[] = { shine };                 // Define shininess value.
    glMaterialfv(GL_FRONT, GL_SPECULAR, mat_specular);   // Apply specular material property.
    glMaterialfv(GL_FRONT, GL_SHININESS, mat_shininess); // Apply shininess material property.
}

// Draws the main booster of the rocket.
void drawMainBooster() {
    if (!mainBooster.isVisible) return; // Only draw if the booster is visible.
    GLUquadric* quad = gluNewQuadric(); // Create a new GLU quadric object for cylinders.
    glPushMatrix();                      // Save the current modelview matrix.
    glTranslatef(mainBooster.position.x, mainBooster.position.y, mainBooster.position.z); // Move to booster's position.
    glRotatef(mainBooster.angle, 1.0f, 0.0f, 0.5f); // Rotate the booster (e.g., for tumble after separation).

    // Draw the main cylindrical body.
    setMaterial(0.8f, 0.8f, 0.8f, 32.0f); // Set material for the rocket body (grey/white).
    glPushMatrix();
    glRotatef(-90.0f, 1.0f, 0.0f, 0.0f); // Rotate cylinder to be vertical along Y-axis.
    gluCylinder(quad, 0.8, 0.8, 4.0, 20, 20); // Draw the cylinder.
    glPopMatrix();

    // Draw the fins.
    setMaterial(0.2f, 0.2f, 1.0f, 16.0f); // Set material for fins (blue).
    for (int i = 0; i < 4; ++i) { // Draw 4 fins.
        glPushMatrix();
        glRotatef(i * 90.0f, 0.0f, 1.0f, 0.0f); // Rotate to position each fin around the body.
        glBegin(GL_TRIANGLES); // Draw a triangular fin.
        glVertex3f(0.8f, 0.2f, 0.0f); glVertex3f(1.5f, -1.0f, 0.0f); glVertex3f(0.8f, -1.0f, 0.0f);
        glEnd();
        glPopMatrix();
    }

    // Draw the exhaust flame if thrusting.
    if (mainBooster.hasThrust) {
        setMaterial(1.0f, 0.6f, 0.1f, 10.0f); // Set material for flame (orange/yellow).
        glPushMatrix();
        glRotatef(90.0f, 1.0f, 0.0f, 0.0f); // Rotate cone to point downwards.
        glutSolidCone(0.6f, 2.0f, 20, 20); // Draw a cone for the flame.
        glPopMatrix();
    }
    glPopMatrix();                       // Restore the previous modelview matrix.
    gluDeleteQuadric(quad);              // Delete the GLU quadric object.
}

// Draws the upper stage of the rocket.
void drawUpperStage() {
    if (!upperStage.isVisible) return; // Only draw if visible.
    GLUquadric* quad = gluNewQuadric(); // Create a new GLU quadric object.
    glPushMatrix();                      // Save the current modelview matrix.
    glTranslatef(upperStage.position.x, upperStage.position.y, upperStage.position.z); // Move to upper stage's position.
    glRotatef(upperStage.angle, 1.0f, 0.0f, 0.0f); // Rotate the upper stage.

    // Draw the main cylindrical body of the upper stage.
    setMaterial(0.8f, 0.8f, 0.8f, 32.0f); // Set material (grey/white).
    glPushMatrix();
    glRotatef(-90.0f, 1.0f, 0.0f, 0.0f); // Rotate cylinder to be vertical.
    gluCylinder(quad, 0.6, 0.6, 2.5, 20, 20); // Draw the cylinder.
    glPopMatrix();

    // Draw the nose cone of the upper stage.
    setMaterial(1.0f, 0.0f, 0.0f, 64.0f); // Set material (red).
    glPushMatrix();
    glTranslatef(0.0f, 2.5f, 0.0f);      // Move to the top of the cylinder.
    glRotatef(-90.0f, 1.0f, 0.0f, 0.0f); // Rotate cone to point upwards.
    glutSolidCone(0.6, 1.0, 20, 20);     // Draw the cone.
    glPopMatrix();

    // Draw the exhaust flame if thrusting.
    if (upperStage.hasThrust) {
        setMaterial(0.5f, 0.8f, 1.0f, 10.0f); // Set material for flame (light blue).
        glPushMatrix();
        glRotatef(90.0f, 1.0f, 0.0f, 0.0f); // Rotate cone to point downwards.
        glutSolidCone(0.4f, 1.5f, 20, 20);  // Draw a cone for the flame.
        glPopMatrix();
    }
    glPopMatrix();                       // Restore the previous modelview matrix.
    gluDeleteQuadric(quad);              // Delete the GLU quadric object.
}

// Draws the satellite.
void drawSatellite() {
    if (!satellite.isVisible) return; // Only draw if visible.
    glPushMatrix();                   // Save the current modelview matrix.
    glTranslatef(satellite.position.x, satellite.position.y, satellite.position.z); // Move to satellite's position.

    // Draw the main body of the satellite (sphere).
    setMaterial(0.9f, 0.9f, 0.1f, 80.0f); // Set material (yellow/gold).
    glutSolidSphere(0.5, 20, 20);         // Draw a solid sphere.

    // Draw the solar panels (cubes scaled flat).
    setMaterial(0.1f, 0.1f, 0.4f, 50.0f); // Set material (dark blue).
    glPushMatrix();
    glScalef(2.5f, 0.5f, 0.1f);           // Scale to create a flat, rectangular panel.
    glutSolidCube(1.0);                   // Draw the first panel.
    glPopMatrix();
    glPushMatrix();
    glScalef(-2.5f, 0.5f, 0.1f);          // Scale for the second panel (symmetrical).
    glutSolidCube(1.0);                   // Draw the second panel.
    glPopMatrix();
    glPopMatrix();                        // Restore the previous modelview matrix.
}

// MODIFIED: Changed drawing technique for continents to be more visible.
// Draws the Earth.
void drawEarth() {
    glPushMatrix();
    glTranslatef(0.0f, WORLD_OFFSET_Y, 0.0f);

    // Enable texturing
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, earthTextureID);

    // Set the material to a neutral color (white) so the texture colors are not tinted.
    setMaterial(1.0f, 1.0f, 1.0f, 20.0f);

    // The GLU quadric will need texture coordinates generated.
    GLUquadric* quad = gluNewQuadric();
    gluQuadricTexture(quad, GL_TRUE); // Enable texture coordinate generation

    // Rotate the Earth so the texture isn't upside down and to give a better initial view
    glRotatef(-90.0f, 1.0f, 0.0f, 0.0f);
    glRotatef(90.0f, 0.0f, 0.0f, 1.0f);

    // Draw the sphere with the texture
    gluSphere(quad, EARTH_RADIUS, 50, 50);

    gluDeleteQuadric(quad); // Delete the GLU quadric object

    // Disable texturing so it doesn't affect other objects
    glDisable(GL_TEXTURE_2D);

    glPopMatrix();
}


// --- Core GLUT Functions ---

// The main display callback function for OpenGL rendering.
void display() {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT); // Clear color and depth buffers.
    glMatrixMode(GL_MODELVIEW);                         // Switch to modelview matrix.
    glLoadIdentity();                                   // Reset the modelview matrix.
    // --- ORBITING CAMERA LOGIC ---
    float cameraDistance = 150.0f; // Original was 110.0f. Increase this to move camera farther.
    float cameraOrbitY = WORLD_OFFSET_Y + 50.0f; // Adjust camera Y to be above the new Earth's center

    float camX = cameraDistance * sin(cameraAngle);
    float camZ = cameraDistance * cos(cameraAngle);

    gluLookAt(camX, 20.0f + WORLD_OFFSET_Y, camZ, // Eye position orbits in a circle
        0.0f, WORLD_OFFSET_Y, 0.0f,         // Look at the center of the Earth
        0.0f, 1.0f, 0.0f);
    // --- END CAMERA LOGIC ---

    drawStars();        // Draw the background stars.
    drawEarth();        // Draw the Earth.
    drawMainBooster();  // Draw the main booster.
    drawUpperStage();   // Draw the upper stage.
    drawSatellite();    // Draw the satellite.
    drawText(missionStatusText, 20.0f, 20.0f); // Draw the mission status text.

    glutSwapBuffers(); // Swap the front and back buffers to display the rendered scene.
}

// Reshape callback function, called when the window is resized.
void reshape(int w, int h) {
    if (h == 0) h = 1; // Prevent division by zero if height is 0.
    glViewport(0, 0, w, h); // Set the viewport to cover the entire window.
    glMatrixMode(GL_PROJECTION); // Switch to projection matrix.
    glLoadIdentity();            // Reset the projection matrix.
    // Set up a perspective projection.
    gluPerspective(45.0f,      // Field of view angle (in degrees).
        (float)w / h, // Aspect ratio (width / height).
        1.0f,       // Near clipping plane.
        500.0);     // Far clipping plane.
    glMatrixMode(GL_MODELVIEW); // Switch back to modelview matrix.
}

// --- Physics and Animation Timer ---

// Timer callback function, called periodically to update simulation physics and state.
void timer(int value) {
    // --- State Machine Logic ---
    switch (currentState) {
    case PRE_LAUNCH:
        missionStatusText = "Mission: Deploy Satellite. Press 'L' to Launch.";
        break;

    case LIFTOFF:
        missionStatusText = "Liftoff! Overcoming gravity's pull.";
        mainBooster.hasThrust = true; // Engage main booster thrust.
        // Check if the rocket has reached the altitude for stage separation.
        if (mainBooster.position.y > (25.0f + WORLD_OFFSET_Y)) {
            currentState = STAGE_SEPARATION;
        }
        break;

    case STAGE_SEPARATION:
        missionStatusText = "Main Booster Separation. Igniting Upper Stage.";
        mainBooster.hasThrust = false; // Disable main booster thrust.
        upperStage.hasThrust = true;   // Engage upper stage thrust.
        mainBooster.velocity.x = -1.0f; // Give the main booster a small sideways push to simulate separation.
        currentState = ORBITAL_INSERTION; // Transition to orbital insertion phase.
        break;

    case ORBITAL_INSERTION:
        missionStatusText = "Orbital Insertion Burn. Pushing to apogee.";
        // Check if the upper stage has reached the target orbital altitude.
        if (upperStage.position.y >= (ORBIT_ALTITUDE + WORLD_OFFSET_Y)) {
            upperStage.hasThrust = false; // Disable upper stage thrust.

            // v = sqrt(GM/r) - calculate required horizontal velocity for a stable circular orbit
            // Here, G is GRAVITATIONAL_CONSTANT, M (mass of Earth) is implicitly included in G.
            // r is the orbital radius, which is ORBIT_ALTITUDE + distance from Earth's center to WORLD_OFFSET_Y.
            // Note: The GRAVITATIONAL_CONSTANT is scaled for the simulation's units and visual effect.
            float orbital_v = sqrt(GRAVITATIONAL_CONSTANT / ORBIT_ALTITUDE);

            // MODIFIED: Set velocity for a stable orbit.
            // Give it a horizontal "kick" and kill vertical velocity.
            upperStage.velocity.x = orbital_v; // Set horizontal velocity for orbit.
            upperStage.velocity.y = 0.0f;      // Zero out vertical velocity.

            currentState = SATELLITE_DEPLOYMENT; // Transition to satellite deployment.
        }
        break;

    case SATELLITE_DEPLOYMENT:
        missionStatusText = "Apogee reached. Deploying satellite.";
        upperStage.isVisible = false; // Make the upper stage invisible (simulating deployment).
        satellite.isVisible = true;   // Make the satellite visible.
        satellite.position = upperStage.position; // Satellite starts at upper stage's position.
        satellite.velocity = upperStage.velocity; // Satellite inherits upper stage's velocity.
        currentState = MISSION_SUCCESS; // Transition to mission success.
        break;

    case MISSION_SUCCESS:
        missionStatusText = "Mission Successful. Satellite is in stable orbit.";
        break;
    }

    // --- Physics and Camera Update ---
    if (currentState > PRE_LAUNCH) { // Only update physics if launch has begun.

        // --- ADDED FOR ORBITING CAMERA ---
        cameraAngle += 0.0005f; // Increment the angle for a slow orbit
        if (cameraAngle > 2.0f * PI) {
            cameraAngle -= 2.0f * PI; // Keep angle in the 0-2PI range
        }
        // ---------------------------------------------

        // Booster Physics (applies as long as main booster is visible)
        if (mainBooster.isVisible) {
            // Calculate direction vector from booster to the center of Earth.
            Vector3D dir_to_earth = { -mainBooster.position.x, (WORLD_OFFSET_Y)-mainBooster.position.y, 0 };
            float dist_sq = dir_to_earth.x * dir_to_earth.x + dir_to_earth.y * dir_to_earth.y; // Squared distance.
            if (dist_sq < 1.0f) dist_sq = 1.0f; // Prevent division by zero near origin.
            float grav_mag = GRAVITATIONAL_CONSTANT / dist_sq; // Calculate gravitational force magnitude.

            // Apply gravitational acceleration.
            mainBooster.velocity.x += (dir_to_earth.x / sqrt(dist_sq)) * grav_mag * TIMESTEP;
            mainBooster.velocity.y += (dir_to_earth.y / sqrt(dist_sq)) * grav_mag * TIMESTEP;

            // Apply thrust if active.
            if (mainBooster.hasThrust) {
                mainBooster.velocity.y += BOOSTER_THRUST * TIMESTEP; // Thrust acts upwards (positive Y).
            }

            // Update position based on velocity.
            mainBooster.position.x += mainBooster.velocity.x * TIMESTEP;
            mainBooster.position.y += mainBooster.velocity.y * TIMESTEP;

            // Check for crash (booster hitting the Earth).
            if (sqrt(dist_sq) < EARTH_RADIUS) {
                mainBooster.isVisible = false; // Make booster invisible if it crashes.
            }

            // Rotate the booster after separation to simulate tumble.
            if (currentState >= STAGE_SEPARATION) mainBooster.angle += 1.0f;
        }

        // Upper Stage Physics (when it's independent after separation)
        if (upperStage.isVisible && currentState >= STAGE_SEPARATION) {
            // Calculate direction vector from upper stage to the center of Earth.
            Vector3D dir_to_earth = { -upperStage.position.x, WORLD_OFFSET_Y - upperStage.position.y, 0 };
            float dist_sq = dir_to_earth.x * dir_to_earth.x + dir_to_earth.y * dir_to_earth.y;
            if (dist_sq < 1.0f) dist_sq = 1.0f;
            float grav_mag = GRAVITATIONAL_CONSTANT / dist_sq;

            // Apply gravitational acceleration.
            upperStage.velocity.x += (dir_to_earth.x / sqrt(dist_sq)) * grav_mag * TIMESTEP;
            upperStage.velocity.y += (dir_to_earth.y / sqrt(dist_sq)) * grav_mag * TIMESTEP;

            // Apply thrust if active.
            if (upperStage.hasThrust) {
                upperStage.velocity.y += UPPER_STAGE_THRUST * TIMESTEP; // Thrust acts upwards.
            }

            // Update position.
            upperStage.position.x += upperStage.velocity.x * TIMESTEP;
            upperStage.position.y += upperStage.velocity.y * TIMESTEP;
        }

        // Lock upper stage to booster during liftoff (they move as one unit).
        if (currentState == LIFTOFF) {
            upperStage.position.x = mainBooster.position.x;
            upperStage.position.y = mainBooster.position.y + 4.0f; // Position upper stage relative to booster.
            upperStage.velocity = mainBooster.velocity; // Inherit booster's velocity.
        }

        // Satellite orbital physics (after deployment)
        if (satellite.isVisible) {
            // Calculate direction vector from satellite to the center of Earth.
            Vector3D dir_to_earth = { -satellite.position.x, WORLD_OFFSET_Y - satellite.position.y, 0 };
            float dist_sq = dir_to_earth.x * dir_to_earth.x + dir_to_earth.y * dir_to_earth.y;
            if (dist_sq < 1.0f) dist_sq = 1.0f;
            float grav_mag = GRAVITATIONAL_CONSTANT / dist_sq;

            // Apply gravitational acceleration.
            satellite.velocity.x += (dir_to_earth.x / sqrt(dist_sq)) * grav_mag * TIMESTEP;
            satellite.velocity.y += (dir_to_earth.y / sqrt(dist_sq)) * grav_mag * TIMESTEP;

            // Update position.
            satellite.position.x += satellite.velocity.x * TIMESTEP;
            satellite.position.y += satellite.velocity.y * TIMESTEP;
        }
    }

    glutPostRedisplay(); // Request a redraw of the scene.
    glutTimerFunc(1000 * TIMESTEP, timer, 0); // Schedule the next timer call.
}

// --- Setup and Control ---

// Keyboard callback function for user input.
void keyboard(unsigned char key, int x, int y) {
    if ((key == 'l' || key == 'L') && currentState == PRE_LAUNCH) {
        currentState = LIFTOFF; // Start the launch when 'L' is pressed in PRE_LAUNCH state.
    }
    if (key == 'r' || key == 'R') {
        resetSimulation(); // Reset the simulation when 'R' is pressed.
    }
    if (key == 27) { // ESC key (ASCII 27)
        exit(0);     // Exit the application.
    }
}

// Generates random positions for stars in a spherical distribution.
void setupStars() {
    stars.clear(); // Clear any existing stars.
    for (int i = 0; i < NUM_STARS; ++i) {
        // Generate spherical coordinates (theta, phi, radius).
        float theta = (rand() / (float)RAND_MAX) * 2.0f * PI; // Azimuthal angle.
        float phi = acos(2.0f * (rand() / (float)RAND_MAX) - 1.0f); // Polar angle (distributes evenly on sphere).
        float radius = 150.0f + (rand() / (float)RAND_MAX) * 50.0f; // Random radius within a range.

        Vector3D star;
        // Convert spherical to Cartesian coordinates.
        star.x = radius * sin(phi) * cos(theta);
        star.y = radius * sin(phi) * sin(theta) + WORLD_OFFSET_Y + 20.0f; // Center stars around scene vertically.
        star.z = radius * cos(phi);
        stars.push_back(star); // Add the new star to the vector.
    }
}

// Resets all game objects and the simulation state to initial values.
void resetSimulation() {
    currentState = PRE_LAUNCH; // Set state back to pre-launch.

    // Initialize main booster properties.
    mainBooster = { {0.0f, EARTH_RADIUS + WORLD_OFFSET_Y, 0.0f}, // Position just above Earth's surface.
                    {0.0f, 0.0f, 0.0f}, // Zero velocity.
                    0.0f,               // Zero angle.
                    true,               // Visible.
                    false };            // No thrust initially.

    // Initialize upper stage properties (starts attached to booster).
    upperStage = { {0.0f, mainBooster.position.y + 4.0f, 0.0f}, // Position relative to booster.
                   {0.0f, 0.0f, 0.0f}, // Zero velocity.
                   0.0f,               // Zero angle.
                   true,               // Visible.
                   false };            // No thrust initially.

    // Initialize satellite properties (starts invisible).
    satellite = { {0.0f, 0.0f, 0.0f}, // Placeholder position.
                  {0.0f, 0.0f, 0.0f}, // Placeholder velocity.
                  0.0f,               // Zero angle.
                  false,              // Not visible initially.
                  false };            // No thrust.
}

// Sets up the initial OpenGL rendering environment and game elements.
void setupScene() {
    glEnable(GL_DEPTH_TEST);  // Enable depth testing for proper 3D rendering.
    glEnable(GL_LIGHTING);    // Enable lighting.
    glEnable(GL_LIGHT0);      // Enable light source 0.
    GLfloat light_pos[] = { 20.0f, 30.0f, 100.0f, 1.0f }; // Define light position.
    glLightfv(GL_LIGHT0, GL_POSITION, light_pos); // Apply light position to light 0.
    glShadeModel(GL_SMOOTH);  // Use smooth shading for interpolated colors.
    glEnable(GL_COLOR_MATERIAL); // Enable color material for simpler color setting.
    glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE); // Apply color to ambient and diffuse properties.
    glClearColor(0.0f, 0.0f, 0.02f, 1.0f); // Set background clear color (dark blue/black for space).
    srand(static_cast<unsigned int>(time(NULL))); // Seed the random number generator with current time.
    loadTexture();
    setupStars();     // Initialize star positions.
    resetSimulation(); // Initialize rocket and satellite positions and states.
}

// Main function: entry point of the program.
int main(int argc, char** argv) {
    glutInit(&argc, argv); // Initialize GLUT.
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH); // Set display mode: double buffer, RGB color, depth buffer.
    glutInitWindowSize(800, 800); // Set initial window size.
    glutCreateWindow("Project Stardust: Satellite Deployment Mission (V2)"); // Create the window with a title.

    setupScene(); // Perform initial scene setup.
    glutDisplayFunc(display); // Register the display callback function.
    glutReshapeFunc(reshape); // Register the reshape callback function.
    glutKeyboardFunc(keyboard); // Register the keyboard callback function.
    glutTimerFunc(0, timer, 0); // Start the timer, calling 'timer' function after 0ms with value 0.
    glutMainLoop(); // Enter the GLUT event processing loop.
    return 0;       // Return 0 to indicate successful execution.
}