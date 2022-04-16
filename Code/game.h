#include <vector>
using std::vector;

const uint16_t SHIP_MATRIX_SIZE = 23;
const uint16_t ENEMY_NR = 8;			//You have to calculate to your own display by the SHIP_WIDTH = 7. On a 64 columns display 8 is good with a SHIP_SPACER = 1
const uint16_t SHIP_SPACER = 1;			//Space between enemy ships

uint16_t gameSpeed = 30;
uint16_t spaceSpeed = 25;
uint16_t textSpeed = 10;
uint16_t explosionSpeed = 35;
uint16_t SHIP_WIDTH = 7;
uint16_t SHIP_HEIGHT = 6;
uint16_t SHIP_CENTER = 3;
uint16_t EXPLOSION_W_H = 5;
uint16_t yStepSpace;
uint16_t shipX = M_WIDTH / 2 - SHIP_CENTER;
uint16_t introX = M_WIDTH;
uint16_t gameOverX1 = M_WIDTH + 10;
uint16_t gameOverX2 = -1 * M_WIDTH - 10;
uint16_t killedEnemy = 0;
uint16_t score = 0, highScore = 0;

uint16_t drawMatrix[M_WIDTH][M_HEIGHT];
uint16_t spaceMatrix[M_WIDTH][M_HEIGHT];
bool isShipMoveLeft = false, isShipMoveRight = false, isEndOfGame = false, isIntro = true, isEndGameTextAnim = false;
long prevSpaceMillis = 0, prevExplosionMillis = 0, prevEnemyMoveMillis = 0, prevShipFireMillis = 0, prevEnemyFireMillis = 0, prevGameOverMillis = 0, prevIntroMillis = 0;

vector<vector<int>> shipMatrix;
vector<vector<int>> enemyShipMatrix =
{
	                          {2,0,red},                {4,0,red},									//Ship jet
	{0,1,ltGray},             {2,1,ltGray},{3,1,ltGray},{4,1,ltGray},             {6,1,ltGray},		//Ship body and wings
	{0,2,ltGray},{1,2,ltGray},{2,2,ltGray},{3,2,ltGray},{4,2,ltGray},{5,2,ltGray},{6,2,ltGray},		//Ship body and wings
	{0,3,ltGray},             {2,3,ltGray},{3,3,ltGray},{4,3,ltGray},             {6,3,ltGray},		//Ship body and wings
	                          {2,4,ltGray},{3,4,ltGray},{4,4,ltGray},								//Ship body
	                                       {3,5,ltGray},											//Ship front
};

vector<vector<vector<int>>> enemies;
vector<vector<vector<int>>> shipRockets;
vector<vector<vector<int>>> enemyRockets;
vector<vector<vector<int>>> explosions;

void sendGameScoreViaBT() {
	SerialBT.println(BT_GAME_CURRENT_SCORE + score);
	delay(10);
	SerialBT.println(BT_GAME_HIGH_SCORE + highScore);
	delay(10);
}

void saveHighScore() {
	preferences.begin(ESP_PREFS, false);
	preferences.putUInt(SPACE_GAME_HIGH_SCORE, highScore);
	preferences.end();
}

void getHighScore() {
	preferences.begin(ESP_PREFS, false);
	highScore = preferences.getUInt(SPACE_GAME_HIGH_SCORE, 0);
	preferences.end();
}

void initSpace() {
	for (int x = 0; x < M_WIDTH; x++) {
		for (int y = 0; y < M_HEIGHT; y++) {
			spaceMatrix[x][y] = black;
			drawMatrix[x][y] = black;
		}
	}
	int xStep = M_WIDTH / 8;
	int yStep = M_HEIGHT / 8;
	int xRand, yRand;
	for (int x = 0; x < M_WIDTH; x+= xStep) {
		for (int y = 0; y < M_HEIGHT; y+= yStep) {
			xRand = random(x, x + xStep);
			yRand = random(y, y + yStep);
			spaceMatrix[xRand][yRand] = stars;
		}
	}
	yStepSpace = 0;
}

void initShip() {
	for (int i = 0; i < shipMatrix.size(); i++) {
		shipMatrix[i][0] = shipMatrix[i][0] + shipX;
		shipMatrix[i][1] = shipMatrix[i][1] + M_HEIGHT - SHIP_HEIGHT;
	}
}

bool isEnemiesMoving() {
	for (vector<vector<int>>& v1 : enemies) {
		for (vector<int>& v2 : v1) {
			if (v2[1] < 0) {
				return true;
			}
		}
	}
	return false;
}

void addEnemy() {
	if (killedEnemy < ENEMY_NR)
		return;

	for (vector<vector<int>>& v1 : explosions) {
		for (vector<int>& v2 : v1) {
			if (v2[2] != -1) {
				return;
			}
		}
	}
	for (vector<vector<int>>& v1 : enemyRockets) {
		for (vector<int>& v2 : v1) {
			if (v2[2] >= 0 && v2[2] < M_HEIGHT) {
				return;
			}
		}
	}
	killedEnemy = 0;
	prevEnemyMoveMillis = 0;
	shipRockets.clear();
	enemies.clear();
	enemyRockets.clear();
	vector<vector<int>> newEnemy;
	uint16_t x = 0;
	for (int i = 0; i < ENEMY_NR; i++) {
		if (i == ENEMY_NR - 1)
			x = M_WIDTH - SHIP_WIDTH;
		newEnemy.clear();
		for (int j = 0; j < enemyShipMatrix.size(); j++) {
			if (i % 2 == 0)
				newEnemy.push_back({ enemyShipMatrix[j][0] + x, enemyShipMatrix[j][1] - SHIP_HEIGHT, enemyShipMatrix[j][2] });
			else
				if (j <= 1)
					newEnemy.push_back({ enemyShipMatrix[j][0] + x, enemyShipMatrix[j][1] - SHIP_HEIGHT, enemyShipMatrix[j][2] });
				else
					newEnemy.push_back({ enemyShipMatrix[j][0] + x, enemyShipMatrix[j][1] - SHIP_HEIGHT, darkGreen });
		}
		enemies.push_back(newEnemy);
		x = x + SHIP_WIDTH + SHIP_SPACER;
	}
	gameSpeed -= 2;
	if (gameSpeed < 10)
		gameSpeed = 10;
	//Serial.println(gameSpeed);
}

void refreshSpace() {
	uint16_t mY = 0;
	for (int y = yStepSpace; y < M_HEIGHT; y++) {
		for (int x = 0; x < M_WIDTH; x++) {
			drawMatrix[x][y] = spaceMatrix[x][mY];
		}
		mY += 1;
	}
	for (int y = 0; y < yStepSpace; y++) {
		for (int x = 0; x < M_WIDTH; x++) {
			if (y < M_HEIGHT)
				drawMatrix[x][y] = spaceMatrix[x][mY];
		}
		mY += 1;
	}

	if (millis() - prevSpaceMillis >= spaceSpeed) {
		prevSpaceMillis = millis();
		yStepSpace += 1;
		if (yStepSpace >= M_HEIGHT) {
			yStepSpace = 0;
		}
	}
}

void enemiesFireing() {
	bool isFound, isEndOfWhile = false;
	int count = 0;
	uint16_t r;
	while (!isEndOfWhile) {
		r = random(0, ENEMY_NR);
		if (r >= ENEMY_NR)
			r = ENEMY_NR - 1;
		vector<vector<int>> i1 = enemies[r];
		if (i1.size() > 0) {
			isEndOfWhile = true;
			for (vector<int>& i2 : i1) {
				if (i2[1] >= 0) {
					isFound = false;
					for (vector<vector<int>>& r1 : enemyRockets) {
						if (!isFound) {
							for (vector<int>& r2 : r1) {
								if (r2[1] > M_HEIGHT) {
									r1.clear();
									r1.push_back({ i1[16][0], SHIP_HEIGHT + 1, white });
									r1.push_back({ i1[16][0], SHIP_HEIGHT + 2, red });
									isFound = true;
								}
							}
						}
					}
					if (!isFound)
						enemyRockets.push_back({ { i1[16][0], SHIP_HEIGHT + 1, white },{ i1[16][0], SHIP_HEIGHT + 2, red } });
					break;
				}
			}
		}
		count += 1;
		if (count == ENEMY_NR * 5) //To avoid never ending loop
			isEndOfWhile = true;
	}
	//Serial.println(enemyRockets.size());
}

void shipFireing() {
	int x = shipX + SHIP_CENTER;
	int y = M_HEIGHT - SHIP_HEIGHT;
	for (vector<vector<int>>& v1 : shipRockets) {
		for (vector<int>& v2 : v1) {
			if (v2[1] < -1) {
				v1.clear();
				v1.push_back({ x, y - 2, blue });
				v1.push_back({ x, y - 1, white });
				return;
			}
		}
	}
	shipRockets.push_back({ {x, y - 2, blue}, {x, y - 1, white} });
	//Serial.println(shipRockets.size());
}

void refreshRockets() {
	for (vector<vector<int>> &v1: shipRockets) {
		for (vector<int>&v2: v1) {
			if (v2[1] > -2) {
				v2[1] = v2[1] - 1;
				if (v2[1] >= 0 && v2[1] < M_HEIGHT)
					try {
					drawMatrix[v2[0]][v2[1]] = v2[2];
					} catch (const std::exception&) {}
			}
		}
	}

	for (vector<vector<int>>& v1 : enemyRockets) {
		for (vector<int>& v2 : v1) {
			if (v2[1] < M_HEIGHT + 1) {
				v2[1] = v2[1] + 1;
				if (v2[1] >= 0 && v2[1] < M_HEIGHT)
					try {
					drawMatrix[v2[0]][v2[1]] = v2[2];
					} catch (const std::exception&) {}
			}
		}
	}
}

void moveLeftShip() {
	if (isShipMoveLeft) {
		if (shipX > 0) {
			shipX -= 1;
			for (int i = 0; i < shipMatrix.size(); i++) {
				shipMatrix[i][0] -= 1;
			}
		}
		isShipMoveLeft = false;
	}
}

void moveRightShip() {
	if (isShipMoveRight) {
		if (shipX < M_WIDTH - SHIP_WIDTH) {
			shipX += 1;
			for (int i = 0; i < shipMatrix.size(); i++) {
				shipMatrix[i][0] += 1;
			}
		}
		isShipMoveRight = false;
	}
}

void refreshShip() {
	for (int i = 0; i < shipMatrix.size(); i++) {
		try {
			drawMatrix[shipMatrix[i][0]][shipMatrix[i][1]] = shipMatrix[i][2];
		} catch (const std::exception&) {}
	}
}

void moveEnemies() {
	if (isEnemiesMoving()) {
		if (millis() - prevEnemyMoveMillis >= spaceSpeed) {
			prevEnemyMoveMillis = millis();
			for (vector<vector<int>>& v1 : enemies) {
				for (vector<int>& v2 : v1) {
					v2[1] += 1;
				}
			}
		}
	}
}

void refreshEnemies() {
	for (vector<vector<int>>& v1 : enemies) {
		for (vector<int>& v2 : v1) {
			if (v2[0] > -1 && v2[1] >= 0 && v2[1] < M_HEIGHT) {
				try {
					drawMatrix[v2[0]][v2[1]] = v2[2];
				} catch (const std::exception&) {}
			}
		}
	}
}

void addExplosion(uint16_t cX, uint16_t cY) {
	if (cX >= 3 && cX <= M_WIDTH - 3 && cY >= 3 && cY <= M_HEIGHT - 3)
	{
		for (vector<vector<int>>& v1 : explosions) {
			for (vector<int>& v2 : v1) {
				if (v2[2] == -1) {
					v1.clear();
					v1.push_back({ cX, cY, 0 });
					return;
				}
			}
		}
		explosions.push_back({ { cX, cY, 0 } });
		//Serial.println(explosions.size());
	}
}

void refreshExplosions() {
	if (millis() - prevExplosionMillis >= explosionSpeed) {
		prevExplosionMillis = millis();
		uint16_t cX, cY;
		for (vector<vector<int>>& v1 : explosions) {
			for (vector<int>& v2 : v1) {
				if (v2[2] != -1) {
					cX = v2[0];
					cY = v2[1];
					try
					{
						switch (v2[2]) {
						case 0:
							drawMatrix[cX][cY] = yellow;

							drawMatrix[cX + 1][cY + 1] = yellow;
							drawMatrix[cX + 1][cY - 1] = yellow;
							drawMatrix[cX - 1][cY + 1] = yellow;
							drawMatrix[cX - 1][cY - 1] = yellow;

							drawMatrix[cX][cY + 1] = yellow;
							drawMatrix[cX][cY - 1] = yellow;
							drawMatrix[cX][cY + 1] = yellow;
							drawMatrix[cX][cY - 1] = yellow;
							v2[2] += 1;
							break;
						case 1:
							drawMatrix[cX][cY] = yellow;

							drawMatrix[cX + 1][cY + 1] = yellow;
							drawMatrix[cX + 1][cY - 1] = yellow;
							drawMatrix[cX - 1][cY + 1] = yellow;
							drawMatrix[cX - 1][cY - 1] = yellow;

							drawMatrix[cX][cY + 1] = yellow;
							drawMatrix[cX][cY - 1] = yellow;
							drawMatrix[cX + 1][cY] = yellow;
							drawMatrix[cX - 1][cY] = yellow;
							v2[2] += 1;
							break;
						case 2:
							drawMatrix[cX][cY] = yellow;

							drawMatrix[cX + 1][cY + 1] = yellow;
							drawMatrix[cX + 1][cY - 1] = yellow;
							drawMatrix[cX - 1][cY + 1] = yellow;
							drawMatrix[cX - 1][cY - 1] = yellow;

							drawMatrix[cX][cY + 1] = yellow;
							drawMatrix[cX][cY - 1] = yellow;
							drawMatrix[cX + 1][cY] = yellow;
							drawMatrix[cX - 1][cY] = yellow;

							drawMatrix[cX + 2][cY + 2] = yellow;
							drawMatrix[cX + 2][cY - 2] = yellow;
							drawMatrix[cX - 2][cY + 2] = yellow;
							drawMatrix[cX - 2][cY - 2] = yellow;

							drawMatrix[cX][cY + 2] = yellow;
							drawMatrix[cX][cY - 2] = yellow;
							drawMatrix[cX + 2][cY] = yellow;
							drawMatrix[cX - 2][cY] = yellow;
							v2[2] += 1;
							break;
						case 3:
							drawMatrix[cX][cY] = yellow;

							drawMatrix[cX + 1][cY + 1] = yellow;
							drawMatrix[cX + 1][cY - 1] = yellow;
							drawMatrix[cX - 1][cY + 1] = yellow;
							drawMatrix[cX - 1][cY - 1] = yellow;

							drawMatrix[cX][cY + 1] = yellow;
							drawMatrix[cX][cY - 1] = yellow;
							drawMatrix[cX + 1][cY] = yellow;
							drawMatrix[cX - 1][cY] = yellow;

							drawMatrix[cX + 2][cY + 2] = yellow;
							drawMatrix[cX + 2][cY - 2] = yellow;
							drawMatrix[cX - 2][cY + 2] = yellow;
							drawMatrix[cX - 2][cY - 2] = yellow;

							drawMatrix[cX][cY + 2] = yellow;
							drawMatrix[cX][cY - 2] = yellow;
							drawMatrix[cX + 2][cY] = yellow;
							drawMatrix[cX - 2][cY] = yellow;

							drawMatrix[cX + 3][cY + 3] = red;
							drawMatrix[cX + 3][cY - 3] = red;
							drawMatrix[cX - 3][cY + 3] = red;
							drawMatrix[cX - 3][cY - 3] = red;

							drawMatrix[cX][cY + 3] = red;
							drawMatrix[cX][cY - 3] = red;
							drawMatrix[cX + 3][cY] = red;
							drawMatrix[cX - 3][cY] = red;
							v2[2] += 1;
							break;
						case 4:
							drawMatrix[cX + 1][cY + 1] = yellow;
							drawMatrix[cX + 1][cY - 1] = yellow;
							drawMatrix[cX - 1][cY + 1] = yellow;
							drawMatrix[cX - 1][cY - 1] = yellow;

							drawMatrix[cX][cY + 1] = yellow;
							drawMatrix[cX][cY - 1] = yellow;
							drawMatrix[cX + 1][cY] = yellow;
							drawMatrix[cX - 1][cY] = yellow;

							drawMatrix[cX + 2][cY + 2] = yellow;
							drawMatrix[cX + 2][cY - 2] = yellow;
							drawMatrix[cX - 2][cY + 2] = yellow;
							drawMatrix[cX - 2][cY - 2] = yellow;

							drawMatrix[cX][cY + 2] = yellow;
							drawMatrix[cX][cY - 2] = yellow;
							drawMatrix[cX + 2][cY] = yellow;
							drawMatrix[cX - 2][cY] = yellow;

							drawMatrix[cX + 3][cY + 3] = red;
							drawMatrix[cX + 3][cY - 3] = red;
							drawMatrix[cX - 3][cY + 3] = red;
							drawMatrix[cX - 3][cY - 3] = red;

							drawMatrix[cX][cY + 3] = red;
							drawMatrix[cX][cY - 3] = red;
							drawMatrix[cX + 3][cY] = red;
							drawMatrix[cX - 3][cY] = red;
							v2[2] += 1;
							break;

						case 5:
							drawMatrix[cX + 2][cY + 2] = yellow;
							drawMatrix[cX + 2][cY - 2] = yellow;
							drawMatrix[cX - 2][cY + 2] = yellow;
							drawMatrix[cX - 2][cY - 2] = yellow;

							drawMatrix[cX][cY + 2] = yellow;
							drawMatrix[cX][cY - 2] = yellow;
							drawMatrix[cX + 2][cY] = yellow;
							drawMatrix[cX - 2][cY] = yellow;

							drawMatrix[cX + 3][cY + 3] = red;
							drawMatrix[cX + 3][cY - 3] = red;
							drawMatrix[cX - 3][cY + 3] = red;
							drawMatrix[cX - 3][cY - 3] = red;

							drawMatrix[cX][cY + 3] = red;
							drawMatrix[cX][cY - 3] = red;
							drawMatrix[cX + 3][cY] = red;
							drawMatrix[cX - 3][cY] = red;
							v2[2] += 1;
							break;

						case 6:
							drawMatrix[cX + 3][cY + 3] = red;
							drawMatrix[cX + 3][cY - 3] = red;
							drawMatrix[cX - 3][cY + 3] = red;
							drawMatrix[cX - 3][cY - 3] = red;

							drawMatrix[cX][cY + 3] = red;
							drawMatrix[cX][cY - 3] = red;
							drawMatrix[cX + 3][cY] = red;
							drawMatrix[cX - 3][cY] = red;
							v2[2] = -1;
							break;
						}
					}
					catch (const std::exception&) {}
				}
			}
		}
	}
}

void checkHit() {
	//Check if your ship rocket hits enemy ship
	for (vector<vector<int>>& v1 : shipRockets) {
		for (vector<int>& v2 : v1) {
			if (v2[1] >= 0) {
				for (vector<vector<int>>& a1 : enemies) {
					for (vector<int>& a2 : a1) {
						if (a2[1] >= 0) {
							if (v2[0] == a2[0] && v2[1] == a2[1]) {
								addExplosion(a1[16][0], a1[16][1]);
								a1.clear();
								if (!isEndOfGame) {
									score += 1;
									killedEnemy += 1;
									if (score >= highScore)
										highScore = score;
									SerialBT.println(BT_GAME_CURRENT_SCORE + score);
								}
								break;
							}
						}
					}
				}
			}
		}
	}
	//Check if your ship rocket hits enemy rocket
	for (vector<vector<int>>& v1 : shipRockets) {
		for (vector<int>& v2 : v1) {
			if (v2[1] >= 0) {
				for (vector<vector<int>>& a1 : enemyRockets) {
					for (vector<int>& a2 : a1) {
						if (a2[1] >= 0) {
							if (v2[0] == a2[0] && v2[1] == a2[1]) {
								if (!isEndOfGame) {
									addExplosion(a2[0], a2[1]);
								}
								a1.clear();
								break;
							}
						}
					}
				}
			}
		}
	}
	//Check if enemy rocket hits your ship
	for (vector<int>& v1 : shipMatrix) {
		for (vector<vector<int>>& a1 : enemyRockets) {
			for (vector<int>& a2 : a1) {
				if (a2[1] >= 0) {
					if (v1[0] == a2[0] && v1[1] == a2[1]) {
						addExplosion(shipMatrix[12][0], shipMatrix[12][1]);
						a1.clear();
						if (!isEndOfGame) {
							if (score >= highScore) {
								saveHighScore();
								sendGameScoreViaBT();
							}
						}
						isEndOfGame = true;
						break;
					}
				}
			}
		}
	}
	if (isEndOfGame) {
		shipMatrix.clear();
		enemies.clear();
		enemyRockets.clear();
		shipRockets.clear();
	}
}

void introText() {
	if (introX > 9) {
		if (millis() - prevIntroMillis >= textSpeed) {
			prevIntroMillis = millis();
			introX -= 1;
		}
	}
	display.setTextWrap(false);
	display.setTextColor(cyan);
	display.setCursor(introX, M_HEIGHT / 2.5);
	display.setTextSize(2);
	display.print("PLAY");
}

void endGameText() {
	isEndGameTextAnim = true;
	if (gameOverX1 > 9) {
		if (millis() - prevGameOverMillis >= textSpeed) {
			prevGameOverMillis = millis();
			gameOverX1 -= 1;
			gameOverX2 += 1;
		}
	}
	else {
		isEndGameTextAnim = false;
	}
	display.setTextWrap(false);
	display.setTextColor(cyan);
	display.setCursor(gameOverX1, M_HEIGHT / 4.25);
	display.setTextSize(2);
	display.print("GAME");

	display.setCursor(gameOverX2, M_HEIGHT / 1.82);
	display.setTextSize(2);
	display.print("OVER");
}

void initGame() {
	isEndGameTextAnim = false;
	isShipMoveLeft = false;
	isShipMoveRight = false;
	killedEnemy = ENEMY_NR;
	prevShipFireMillis = 0;
	prevEnemyFireMillis = 0;
	prevGameOverMillis = 0;
	gameSpeed = 32;
	enemies.clear();
	shipRockets.clear();
	enemyRockets.clear();
	explosions.clear();

	introX = M_WIDTH;
	gameOverX1 = M_WIDTH + 20;
	gameOverX2 = -1 * M_WIDTH - 2;
	shipX = M_WIDTH / 2 - SHIP_CENTER;
	prevSpaceMillis = 0;
	prevExplosionMillis = 0;
	initSpace();
	shipMatrix =
	{
		                                       {3,0,yellow},											//Ship front
		                          {2,1,yellow},{3,1,yellow},{4,1,yellow},								//Ship body
		{0,2,yellow},             {2,2,yellow},{3,2,yellow},{4,2,yellow},             {6,2,yellow},		//Ship body and wings
		{0,3,yellow},{1,3,yellow},{2,3,yellow},{3,3,yellow},{4,3,yellow},{5,3,yellow},{6,3,yellow},		//Ship body and wings
		{0,4,yellow},             {2,4,yellow},{3,4,yellow},{4,4,yellow},             {6,4,yellow},		//Ship body and wings
		                          {2,5,blue},               {4,5,blue}									//Ship jet
	};
	initShip();
	score = 0;
	getHighScore();
	sendGameScoreViaBT();
	isEndOfGame = false;
}

void playGame() {
	if (isGame) {
		for (int x = 0; x < M_WIDTH; x++) {
			for (int y = 0; y < M_HEIGHT; y++) {
				drawMatrix[x][y] = black;
			}
		}
		if (!isEndOfGame) {
			addEnemy();
			moveEnemies();
		}
		if (!isEndOfGame && !isIntro) {
			try {
				moveLeftShip();
				moveRightShip();
			}
			catch (const std::exception&) {}
		}
		refreshSpace();
		refreshShip();
		refreshEnemies();
		if (!isEndOfGame && !isIntro) {
			refreshRockets();
			checkHit();
		}
		refreshExplosions();
		display.clearDisplay();
		display.setBrightness(brightness);
		for (int x = 0; x < M_WIDTH; x++) {
			for (int y = 0; y < M_HEIGHT; y++) {
				display.drawPixel(x, y, drawMatrix[x][y]);
			}
		}
		if (isEndOfGame)
			endGameText();

		if (isIntro)
			introText();

		display.showBuffer();
		delay(1); //I think this delay is important here to avoid occasionally crash
		if (isGame) {
			if (!isEndOfGame && !isIntro) {
				if (millis() - prevShipFireMillis >= 10 * gameSpeed) {
					prevShipFireMillis = millis();
					if (!isEnemiesMoving())
						shipFireing();
				}
			}
		}

		if (isGame) {
			if (!isEndOfGame && !isIntro) {
				if (millis() - prevEnemyFireMillis >= 35 * gameSpeed) {
					prevEnemyFireMillis = millis();
					if (!isEnemiesMoving())
						enemiesFireing();
				}
			}
		}
	}
}