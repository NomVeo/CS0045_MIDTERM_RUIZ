#define GL_SILENCE_DEPRECATION
#ifdef __APPLE__
#include <GLUT/glut.h>
#else
#include <GL/glut.h>
#include <GL/freeglut_ext.h>
#endif

#include <cmath>
#include <cstdio>
#include <iostream>
#include <cstdlib>

#ifdef _WIN32
#include <windows.h>
#endif

using namespace std;

// Define the fixed window size and number of background stars.
const int WINDOW_WIDTH = 800;
const int WINDOW_HEIGHT = 600;
const int STAR_COUNT = 100;

// Stores the player's position, movement speed, hitbox, and remaining lives.
struct Player
{
    float x, y;
    float speed;
    float hitboxRadius;
    int lives;
};

// Stores the boss position and health values.
struct Boss
{
    float x, y;
    int health;
    int maxHealth;
};

// Stores one background star's position and visual size.
struct Star
{
    float x, y;
    float size;
};

// Stores the movement and state shared by player and enemy bullets.
struct Bullet
{
    float x, y;
    float vx, vy;
    float radius;
    float angle;
    float lifetime;
    float homingTime;
    bool homing;
    bool active;
};

const int MAX_PLAYER_BULLETS = 50;
const int MAX_ENEMY_BULLETS = 150;

// Fixed-size arrays reuse bullet slots instead of allocating during gameplay.
Bullet playerBullets[MAX_PLAYER_BULLETS];
Bullet enemyBullets[MAX_ENEMY_BULLETS];

// Position and size of the clickable button on the start menu.
float startButtonX = 300.0f;
float startButtonY = 260.0f;
float startButtonWidth = 200.0f;
float startButtonHeight = 55.0f;

enum GameState
{
    MENU,
    PLAYING,
    PAUSED,
    VICTORY,
    GAME_OVER
};

// Track the current screen and input states.
GameState gameState = MENU;
bool keys[256] = {false};
bool shiftHeld = false;
bool mouseLeftHeld = false;

float shootTimer = 0.0f;
// Minimum time in seconds between player shots.
const float SHOOT_INTERVAL = 0.18f;

float attackTimer = 0.0f;
// Minimum time in seconds between boss attacks.
const float ATTACK_INTERVAL = 0.6f;

// Selects which boss attack will be used next.
int currentPattern = 0;

float damageCooldown = 0.0f;
// Prevents one collision from removing several lives immediately.
const float DAMAGE_COOLDOWN = 1.0f;

// Create the player and boss with their starting values.
Player player = {400.0f, 100.0f, 250.0f, 5.0f, 3};

Boss boss = {400.0f, 480.0f, 300, 300};

Star stars[STAR_COUNT];

// Define the player ship as one triangle centered around the origin.
GLfloat playerVertices[] = {
    0.0f, 24.0f,    // Top
    -18.0f, -12.0f, // Bottom left
    18.0f, -12.0f   // Bottom right
};

// Define the boss outline around a center vertex.
GLfloat bossVertices[] = {
    0.0f, 0.0f,     // Center
    0.0f, 45.0f,    // Top
    39.0f, 22.5f,   // Upper right
    39.0f, -22.5f,  // Lower right
    0.0f, -45.0f,   // Bottom
    -39.0f, -22.5f, // Lower left
    -39.0f, 22.5f,  // Upper left
    0.0f, 45.0f     // Repeat first outer point
};

// Join the boss vertices into six triangles.
GLubyte bossIndices[] = {
    0, 1, 2,
    0, 2, 3,
    0, 3, 4,
    0, 4, 5,
    0, 5, 6,
    0, 6, 1
};

// Draw bitmap text starting at the given screen position.
void drawText(float x, float y, const char *text){
    // Set the starting position for the first character.
    glRasterPos2f(x, y);

    // Draw one character at a time until the string ends.
    while (*text != '\0')
    {
        glutBitmapCharacter(GLUT_BITMAP_8_BY_13, *text);
        text++;
    }
}

// Draw the title, start button, and controls shown before gameplay.
void drawStartMenu(){

    // Game title
    glColor3f(1.0f, 1.0f, 1.0f);

    drawText(320.0f, 430.0f, "PROJECT: STARPOINT");
    drawText(300.0f, 360.0f, "Defeat Midboss and survive!");

    // Start button
    glColor3f(0.10f, 0.35f, 0.80f);

    glBegin(GL_QUADS);
        glVertex2f(startButtonX, startButtonY);
        glVertex2f(startButtonX + startButtonWidth, startButtonY);
        glVertex2f(startButtonX + startButtonWidth, startButtonY + startButtonHeight);
        glVertex2f(startButtonX, startButtonY + startButtonHeight);
    glEnd();

    // Button label

    glColor3f(1.0f, 1.0f, 1.0f);
    drawText(355.0f, 282.0f, "START GAME");

    // Instructions
    drawText(300.0f, 210.0f, "WASD - Move");
    drawText(300.0f, 190.0f, "Mouse 1 - Shoot");
    drawText(300.0f, 170.0f, "Hold SHIFT - Precision Movement");
    drawText(300.0f, 150.0f, "P - Pause");
    drawText(300.0f, 130.0f, "R - Restart");
}

// Clear the frame and draw the star field and arena border.
void drawBackground(){

    // Space background
    glClearColor(0.015f, 0.02f, 0.08f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    // Draw stars

    glPointSize(2.0f);
    glBegin(GL_POINTS);

    for (int i = 0; i < STAR_COUNT; i++)
    {
        glColor3f(0.6f, 0.75f, 1.0f);
        glVertex2f(stars[i].x, stars[i].y);
    }

    glEnd();

    // Arena border

    glColor3f(0.15f, 0.35f, 0.65f);
    glLineWidth(2.0f);

    glBegin(GL_LINE_LOOP);
        glVertex2f(20.0f, 20.0f);
        glVertex2f(780.0f, 20.0f);
        glVertex2f(780.0f, 580.0f);
        glVertex2f(20.0f, 580.0f);
    glEnd();
}

// Draw a filled circle using a center point and 40 edge points.
void drawCircle(float x, float y, float radius)
{
    glBegin(GL_TRIANGLE_FAN);

    // The first vertex is the center of the triangle fan.
    glVertex2f(x, y);

    for (int i = 0; i <= 40; i++)
    {
        float angle = 2.0f * 3.14159265f * i / 40.0f;

        glVertex2f(
            x + radius * cos(angle),
            y + radius * sin(angle));
    }

    glEnd();
}

// Draw the player ship, thrusters, and optional precision hitbox.
void drawShip()
{

    // Ship hull rendered from a vertex array

    glColor3f(0.10f, 0.70f, 1.0f);
    glEnableClientState(GL_VERTEX_ARRAY);
    glVertexPointer(2, GL_FLOAT, 0, playerVertices);

    glPushMatrix();
    glTranslatef(player.x, player.y, 0.0f);

    glDrawArrays(GL_TRIANGLES, 0, 3);
    glPopMatrix();

    glDisableClientState(GL_VERTEX_ARRAY);

    // Cockpit
    glColor3f(0.20f, 0.90f, 1.0f);

    glBegin(GL_TRIANGLES);
        glVertex2f(player.x, player.y + 12.0f);
        glVertex2f(player.x - 6.0f, player.y - 2.0f);
        glVertex2f(player.x + 6.0f, player.y - 2.0f);
    glEnd();

    // Left thruster

    glColor3f(1.0f, 0.5f, 0.0f);

    glBegin(GL_TRIANGLES);
        glVertex2f(player.x - 16.0f, player.y - 10.0f);
        glVertex2f(player.x - 9.0f, player.y - 10.0f);
        glVertex2f(player.x - 13.0f, player.y - 20.0f);
    glEnd();

    // Right thruster

    glBegin(GL_TRIANGLES);
        glVertex2f(player.x + 9.0f, player.y - 10.0f);
        glVertex2f(player.x + 16.0f, player.y - 10.0f);
        glVertex2f(player.x + 13.0f, player.y - 20.0f);
    glEnd();

    if (shiftHeld)
    {
        // Show the exact hitbox while precision movement is active.
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glColor4f(1.0f, 0.0f, 0.0f, 1.0f);
        drawCircle(player.x, player.y, player.hitboxRadius);
        glDisable(GL_BLEND);
    }
}

// Draw the boss body and eye while it is still alive.
void drawBoss()
{
    if (boss.health <= 0)
    {
        // Do not render the boss after it has been defeated.
        return;
    }

    // Boss body using indexed geometry
    glColor3f(0.55f, 0.12f, 0.75f);

    glPushMatrix();
    glTranslatef(boss.x, boss.y, 0.0f);

    glEnableClientState(GL_VERTEX_ARRAY);

    glVertexPointer(2, GL_FLOAT, 0, bossVertices);

    glDrawElements(
        GL_TRIANGLES,
        18,
        GL_UNSIGNED_BYTE,
        bossIndices
    );

    glDisableClientState(GL_VERTEX_ARRAY);
    glPopMatrix();

    // White eyeball
    glColor3f(1.0f, 1.0f, 1.0f);
    drawCircle(boss.x, boss.y, 27.0f);

    // Red iris
    glColor3f(1.0f, 0.1f, 0.15f);
    drawCircle(boss.x, boss.y, 15.0f);

    // Dark pupil
    glColor3f(0.05f, 0.02f, 0.08f);
    drawCircle(boss.x, boss.y, 7.0f);

    // Small eye highlight
    glColor3f(1.0f, 1.0f, 1.0f);
    drawCircle(boss.x - 5.0f, boss.y + 6.0f, 3.0f);
}

// Draw the boss health bar based on its remaining health.
void drawHealthBar()
{
    float barWidth = 250.0f;
    // Convert the health value into a fraction used to scale the bar.
    float healthRatio = static_cast<float>(boss.health) / boss.maxHealth;

    // Background of the health bar
    glColor3f(0.25f, 0.05f, 0.1f);

    glBegin(GL_QUADS);
    glVertex2f(275.0f, 545.0f);
    glVertex2f(525.0f, 545.0f);
    glVertex2f(525.0f, 560.0f);
    glVertex2f(275.0f, 560.0f);
    glEnd();

    // Current health
    glColor3f(1.0f, 0.1f, 0.3f);

    glBegin(GL_QUADS);
    glVertex2f(275.0f, 545.0f);
    glVertex2f(275.0f + barWidth * healthRatio, 545.0f);
    glVertex2f(275.0f + barWidth * healthRatio, 560.0f);
    glVertex2f(275.0f, 560.0f);
    glEnd();

    drawText(370.0f, 568.0f, "MIDBOSS");
}

// Draw the game title, lives, and control reminders.
void drawHUD()
{
    glColor3f(1.0f, 1.0f, 1.0f);
    drawText(35.0f, 555.0f, "PROJECT: STARPOINT");

    char livesText[30];

    // Build a text string that includes the current number of lives.
    sprintf(livesText, "LIVES: %d", player.lives);
    drawText(35.0f, 530.0f, livesText);
    drawText(35.0f, 85.0f, "WASD: MOVE");
    drawText(35.0f, 65.0f, "MOUSE 1: SHOOT");
    drawText(35.0f, 45.0f, "SHIFT: PRECISION MOVEMENT");
    drawText(35.0f, 25.0f, "P: PAUSE   R: RESTART");
}

// Draw every active player bullet as a cyan point.
void drawPlayerBullets()
{
    glPointSize(5.0f);
    glColor3f(0.2f, 1.0f, 1.0f);
    glBegin(GL_POINTS);

    for (int i = 0; i < MAX_PLAYER_BULLETS; i++)
    {

        if (playerBullets[i].active)
        {
            glVertex2f(playerBullets[i].x, playerBullets[i].y);
        }
    }
    glEnd();
}

// Draw active enemy bullets with a color based on their behavior.
void drawEnemyBullets()
{
    glPointSize(7.0f);
    glBegin(GL_POINTS);

    for (int i = 0; i < MAX_ENEMY_BULLETS; i++)
    {

        if (enemyBullets[i].active)
        {
            if (enemyBullets[i].homing)
            {
                // Homing bullet: magenta
                glColor3f(1.0f, 0.35f, 0.85f);
            }
            else
            {
                // Fan bullet: red
                glColor3f(1.0f, 0.2f, 0.2f);
            }
            glVertex2f(enemyBullets[i].x, enemyBullets[i].y);
        }
    }
    glEnd();
}

// Draw the message for victory, game over, or pause states.
void drawGameMessage(){

    if (gameState == VICTORY)
    {
        glColor3f(0.2f, 1.0f, 0.4f);
        drawText(350.0f, 330.0f, "VICTORY!");
        glColor3f(1.0f, 1.0f, 1.0f);
        drawText(370.0f, 295.0f, "MIDBOSS DEFEATED");
        drawText(370.0f, 260.0f, "Press R to restart");
    }

    else if (gameState == GAME_OVER)
    {

        glColor3f(1.0f, 0.2f, 0.2f);

        drawText(370.0f, 330.0f, "GAME OVER");
        glColor3f(1.0f, 1.0f, 1.0f);
        drawText(300.0f, 295.0f, "Your ship was destroyed");
        drawText(310.0f, 260.0f, "Press R to restart");
    }

    else if (gameState == PAUSED)
    {
        glColor3f(1.0f, 1.0f, 0.3f);
        drawText(365.0f, 330.0f, "PAUSED");
        glColor3f(1.0f, 1.0f, 1.0f);
        drawText(300.0f, 295.0f, "Press P to continue");
    }
}

// Render one complete frame based on the current game state.
void display(){
    drawBackground();

    if (gameState == MENU)
    {
        drawStartMenu();
    }
    else
    {
        drawBoss();
        drawShip();
        drawPlayerBullets();
        drawEnemyBullets();
        drawHealthBar();
        drawHUD();
        drawGameMessage();
    }
    // Present the completed back buffer to avoid visible flickering.
    glutSwapBuffers();
}

// Give every background star a random position and size.
void initializeStars(){
    for (int i = 0; i < STAR_COUNT; i++)
    {
        stars[i].x = static_cast<float>(rand() % WINDOW_WIDTH);
        stars[i].y = static_cast<float>(rand() % WINDOW_HEIGHT);

        stars[i].size = 1.0f + (rand() % 2);
    }
}

// Read movement input and update the player's position.
void updatePlayer(float deltaTime) {

    // Detect Shift
    shiftHeld = (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;

    // Movement speed
    float moveSpeed = player.speed;

    if (shiftHeld) {
        // Slow the ship for more precise movement near bullets.
        moveSpeed *= 0.4f;
    }

    float dx = 0.0f;
    float dy = 0.0f;

    // Detect physical WASD keys
    if (GetAsyncKeyState('A') & 0x8000)
        dx -= 1.0f;

    if (GetAsyncKeyState('D') & 0x8000)
        dx += 1.0f;

    if (GetAsyncKeyState('S') & 0x8000)
        dy -= 1.0f;

    if (GetAsyncKeyState('W') & 0x8000)
        dy += 1.0f;

    // Normalize diagonal movement
    float length = sqrt(dx * dx + dy * dy);

    if (length > 0.0f) {
        dx /= length;
        dy /= length;
    }

    // Update position
    // Multiplying by deltaTime keeps movement speed stable over time.
    player.x += dx * moveSpeed * deltaTime;
    player.y += dy * moveSpeed * deltaTime;

    // Screen boundaries
    if (player.x < 40.0f)
        player.x = 40.0f;

    if (player.x > WINDOW_WIDTH - 40.0f)
        player.x = WINDOW_WIDTH - 40.0f;

    if (player.y < 45.0f)
        player.y = 45.0f;

    if (player.y > 250.0f)
        player.y = 250.0f;
}

// Activate the first available player bullet slot.
void spawnPlayerBullet(){

    for (int i = 0; i < MAX_PLAYER_BULLETS; i++)
    {

        if (!playerBullets[i].active)
        {
            // Start the bullet just above the player's ship.
            playerBullets[i].x = player.x;
            playerBullets[i].y = player.y + 20.0f;

            playerBullets[i].vx = 0.0f;
            playerBullets[i].vy = 450.0f;

            playerBullets[i].radius = 3.0f;
            playerBullets[i].angle = 1.5707963f;

            playerBullets[i].lifetime = 3.0f;
            playerBullets[i].homingTime = 0.0f;

            playerBullets[i].homing = false;
            playerBullets[i].active = true;

            // Only one bullet is created for each call.
            return;
        }
    }
}

// Activate an enemy bullet with the requested horizontal and vertical speed.
void spawnEnemyBullet(float vx, float vy){
    for (int i = 0; i < MAX_ENEMY_BULLETS; i++)
    {

        if (!enemyBullets[i].active)
        {
            enemyBullets[i].x = boss.x;
            enemyBullets[i].y = boss.y - 40.0f;

            enemyBullets[i].vx = vx;
            enemyBullets[i].vy = vy;

            enemyBullets[i].radius = 5.0f;

            enemyBullets[i].angle = atan2(vy, vx);

            enemyBullets[i].lifetime = 5.0f;

            enemyBullets[i].homingTime = 0.0f;

            enemyBullets[i].homing = false;

            enemyBullets[i].active = true;

            return;
        }
    }
}

// Move player bullets and deactivate them after they leave the screen.
void updatePlayerBullets(float deltaTime){

    for (int i = 0; i < MAX_PLAYER_BULLETS; i++)
    {
        if (!playerBullets[i].active)
        {
            continue;
        }

        playerBullets[i].x += playerBullets[i].vx * deltaTime;

        playerBullets[i].y += playerBullets[i].vy * deltaTime;

        if (playerBullets[i].y > WINDOW_HEIGHT)
        {
            playerBullets[i].active = false;
        }
    }
}

// Update enemy movement, homing behavior, lifetime, and bounds.
void updateEnemyBullets(float deltaTime){
    const float PI = 3.14159265f;
    const float HOMING_SPEED = 175.0f;
    const float MAX_TURN_RATE = PI;

    for (int i = 0; i < MAX_ENEMY_BULLETS; i++)
    {
        Bullet &bullet = enemyBullets[i];
        if (!bullet.active)
        {
            continue;
        }

        // Homing bullets gradually turn toward the player

        if (bullet.homing && bullet.homingTime > 0.0f)
        {
            float dx = player.x - bullet.x;
            float dy = player.y - bullet.y;
            float targetAngle = atan2(dy, dx);
            float difference = targetAngle - bullet.angle;

            // Wrap the angle so the bullet turns along the shortest path.
            while (difference > PI)
                difference -= 2.0f * PI;

            while (difference < -PI)
                difference += 2.0f * PI;

            float maxTurn = MAX_TURN_RATE * deltaTime;

            // Limit how far the bullet can turn during this frame.
            if (difference > maxTurn)
                difference = maxTurn;

            if (difference < -maxTurn)
                difference = -maxTurn;

            bullet.angle += difference;

            bullet.vx = cos(bullet.angle) * HOMING_SPEED;
            bullet.vy = sin(bullet.angle) * HOMING_SPEED;

            bullet.homingTime -= deltaTime;

            if (bullet.homingTime <= 0.0f)
            {
                bullet.homingTime = 0.0f;
                bullet.homing = false;
            }
        }

        // Move the bullet

        bullet.x += bullet.vx * deltaTime;
        bullet.y += bullet.vy * deltaTime;

        bullet.lifetime -= deltaTime;

        // Remove expired or out-of-bounds bullets

        if (
            bullet.lifetime <= 0.0f ||
            bullet.y < 0.0f ||
            bullet.y > WINDOW_HEIGHT ||
            bullet.x < 0.0f ||
            bullet.x > WINDOW_WIDTH

        )
        {
            bullet.active = false;
        }
    }
}

// Fire one bullet that follows the player for a limited time.
void homingShot(){

    for (int i = 0; i < MAX_ENEMY_BULLETS; i++)
    {

        if (!enemyBullets[i].active)
        {
            float bulletX = boss.x;
            float bulletY = boss.y - 40.0f;

            float dx = player.x - bulletX;
            float dy = player.y - bulletY;

            enemyBullets[i].x = bulletX;
            enemyBullets[i].y = bulletY;

            enemyBullets[i].angle = atan2(dy, dx);

            // Convert the aiming angle into horizontal and vertical speed.
            enemyBullets[i].vx = cos(enemyBullets[i].angle) * 175.0f;
            enemyBullets[i].vy = sin(enemyBullets[i].angle) * 175.0f;

            enemyBullets[i].radius = 6.0f;
            enemyBullets[i].lifetime = 4.0f;
            enemyBullets[i].homingTime = 2.0f;

            enemyBullets[i].homing = true;
            enemyBullets[i].active = true;

            return;
        }
    }
}

// Fire a wide fan of bullets below the boss.
void splitShot(){

    const float angles[] = {-49.0f, -42.0f, -35.0f, -28.0f, -21.0f,
                            -14.0f, -7.0f, 0.0f, 7.0f, 14.0f,
                            21.0f, 28.0f, 35.0f, 42.0f, 49.0f};

    const float speed = 275.0f;

    for (int i = 0; i < 15; i++)
    {
        // Convert degrees to radians before using sine and cosine.
        float radians = angles[i] * 3.14159265f / 180.0f;
        float vx = speed * sin(radians);
        float vy = -speed * cos(radians);

        spawnEnemyBullet(vx, vy);
    }
}

// Trigger boss attacks at a fixed interval and alternate their pattern.
void updateBossAttacks(float deltaTime){
    attackTimer += deltaTime;

    if (attackTimer >= ATTACK_INTERVAL)
    {
        attackTimer = 0.0f;

        if (currentPattern == 0)
        {
            homingShot();
            currentPattern = 1;
        }

        else
        {
            splitShot();
            currentPattern = 0;
        }
    }
}

// Return true when two circular hitboxes overlap.
bool checkCollision(float x1, float y1, float r1, float x2, float y2, float r2)
{
    float dx = x1 - x2;
    float dy = y1 - y2;

    float distanceSquared = dx * dx + dy * dy;
    float radiusSum = r1 + r2;

    // Compare squared values to avoid an unnecessary square-root calculation.
    return distanceSquared <= radiusSum * radiusSum;
}

// Damage the boss when an active player bullet touches it.
void checkPlayerBulletCollisions(){

    for (int i = 0; i < MAX_PLAYER_BULLETS; i++)
    {

        if (!playerBullets[i].active)
        {

            continue;
        }

        if (checkCollision(

                playerBullets[i].x,

                playerBullets[i].y,

                playerBullets[i].radius,

                boss.x,

                boss.y,

                45.0f

                ))
        {

            // Remove the bullet so it can only damage the boss once.
            playerBullets[i].active = false;

            boss.health -= 5;

            if (boss.health < 0)
            {

                boss.health = 0;
            }

            if (boss.health == 0)
            {
                // End active gameplay as soon as the boss is defeated.
                gameState = VICTORY;
            }
        }
    }
}

// Remove a life when an enemy bullet touches the player.
void checkEnemyBulletCollisions(){

    if (

        damageCooldown > 0.0f ||

        gameState != PLAYING

    )
    {
        // Ignore collisions while invulnerable or outside active gameplay.
        return;
    }

    for (int i = 0; i < MAX_ENEMY_BULLETS; i++)
    {

        if (!enemyBullets[i].active)
        {

            continue;
        }

        if (checkCollision(enemyBullets[i].x,

                           enemyBullets[i].y,

                           enemyBullets[i].radius,

                           player.x,

                           player.y,

                           player.hitboxRadius

                           ))
        {

            enemyBullets[i].active = false;
            player.lives--;
            // Give the player temporary protection after taking damage.
            damageCooldown = DAMAGE_COOLDOWN;

            if (player.lives <= 0)
            {
                player.lives = 0;
                gameState = GAME_OVER;
            }
            break;
        }
    }
}

// Restore all gameplay values and clear active input and bullets.
void resetGame()
{
    player.x = 400.0f;
    player.y = 100.0f;
    player.lives = 3;

    boss.health = boss.maxHealth;

    shootTimer = 0.0f;
    attackTimer = 0.0f;
    currentPattern = 0;
    damageCooldown = 0.0f;
    mouseLeftHeld = false;

    for (int i = 0; i < 256; i++)
    {
        keys[i] = false;
    }

    for (int i = 0; i < MAX_PLAYER_BULLETS; i++)
    {
        playerBullets[i].active = false;
    }

    for (int i = 0; i < MAX_ENEMY_BULLETS; i++)
    {
        enemyBullets[i].active = false;
    }
}

// Handle key presses for quitting, pausing, and restarting.
void keyboardDown(unsigned char key, int x, int y)
{
    keys[key] = true;

    if (key == 27)
    {
        // The Escape key closes the program.
        exit(0);
    }
    // Pause and resume

    if (key == 'p' || key == 'P')
    {

        if (gameState == PLAYING)
        {
            gameState = PAUSED;
        }
        else if (gameState == PAUSED)
        {
            gameState = PLAYING;
        }
    }

    // Restart, except from the start menu

    if ((key == 'r' || key == 'R') && gameState != MENU)
    {
        resetGame();
        gameState = PLAYING;
    }
}

// Mark a key as released so it no longer counts as held.
void keyboardUp(unsigned char key, int x, int y) { 
    keys[key] = false; 
}

// Handle start-button clicks and continuous shooting input.
void mouseButton(int button, int state, int x, int y){

    if (button != GLUT_LEFT_BUTTON)
    {
        return;
    }

    if (state == GLUT_UP)
    {
        mouseLeftHeld = false;
        return;
    }

    // Start button

    if (gameState == MENU)
    {

        float mouseX = static_cast<float>(x);
        // Convert GLUT's top-left mouse origin to OpenGL's bottom-left origin.
        float mouseY = WINDOW_HEIGHT - static_cast<float>(y);

        if (mouseX >= startButtonX && mouseX <= startButtonX + startButtonWidth &&
            mouseY >= startButtonY && mouseY <= startButtonY + startButtonHeight)
        {
            resetGame();
            gameState = PLAYING;
        }
        mouseLeftHeld = false;
    }

    else if (gameState == PLAYING)
    {
        mouseLeftHeld = true;
    }
}

// Advance the game by one fixed update and request the next frame.
void timer(int value){
    // A 16 ms step targets approximately 60 updates per second.
    const float deltaTime = 0.016f;

#ifdef _WIN32
    shiftHeld = (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;
#endif

    if (gameState == PLAYING)
    {
        updatePlayer(deltaTime);

        // Continuous shooting while mouse is held
        shootTimer += deltaTime;

        if (mouseLeftHeld && shootTimer >= SHOOT_INTERVAL

        )
        {
            spawnPlayerBullet();
            shootTimer = 0.0f;
        }

        updatePlayerBullets(deltaTime);
        updateEnemyBullets(deltaTime);
        updateBossAttacks(deltaTime);

        checkPlayerBulletCollisions();
        checkEnemyBulletCollisions();

        // Damage cooldown

        if (damageCooldown > 0.0f)
        {
            damageCooldown -= deltaTime;

            if (damageCooldown < 0.0f)
            {
                damageCooldown = 0.0f;
            }
        }
    }
    // Request a redraw and schedule this function to run again.
    glutPostRedisplay();
    glutTimerFunc(16, timer, 0);
}

// Initialize OpenGL, register callbacks, and start the GLUT event loop.
int main(int argc, char **argv){
    // Use a fixed seed so the star layout is repeatable.
    srand(1);
    glutInit(&argc, argv);

    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB);

    glutInitWindowSize(WINDOW_WIDTH, WINDOW_HEIGHT);
    glutInitWindowPosition(100, 50);

    glutCreateWindow("Project: StarPoint");

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();

    // Match OpenGL coordinates to the 800 by 600 game window.
    gluOrtho2D(0.0, WINDOW_WIDTH, 0.0, WINDOW_HEIGHT);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    initializeStars();

    // Connect GLUT events to the game's drawing and input functions.
    glutDisplayFunc(display);
    glutKeyboardFunc(keyboardDown);
    glutKeyboardUpFunc(keyboardUp);
    glutMouseFunc(mouseButton);

    glutTimerFunc(16, timer, 0);

    // Keep processing window events until the program closes.
    glutMainLoop();
    return 0;
}
