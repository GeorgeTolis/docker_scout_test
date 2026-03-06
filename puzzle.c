#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>
#include <stdbool.h>
#include <unistd.h>
#include <raylib.h>

typedef unsigned **Puzzle;

//screen dimensions
const int screenWidth = 800;
const int screenHeight = 800;

//Checks for failed allocations
void checkMalloc(void *ptr){
    if (ptr == NULL){
        fprintf(stderr, "Failed to allocate with malloc\n");
        exit(1);
    }
}

//Checks if given puzzle is the solution
bool isSolution(Puzzle puzzle, int N){
    int i, j;
    if (puzzle[0][0] == 0){
        for (i = 0; i < N; i++)
            for (j = 0; j < N; j++)
                if (puzzle[i][j] != i*N + j) return false;
        return true;
    } else {
        for (i = 0; i < N; i++)
            for (j = 0; j < N; j++){
                if (i == N-1 && j == N-1) break;
                if (puzzle[i][j] != i*N + j + 1) return false;
            }       
        return true; 
    }      
}

//Finds the 0 in the puzzle array
void findBlank(Puzzle puzzle, int N, int *i, int *j){
    for (*i = 0; *i < N; (*i)++)
        for (*j = 0; *j < N; (*j)++)
            if (puzzle[*i][*j] == 0)
                return;
}

//Swaps two values
void swap(unsigned *x, unsigned *y){
    unsigned temp = *x;
    *x = *y;
    *y = temp;
}

//Draws window using raylib
void drawWindow(Puzzle puzzle, int N){
    int i, j;
    BeginDrawing();

        ClearBackground(RAYWHITE);

        for (i = 0; i < N; i++){
            for (j = 0; j < N; j++){
                if (!puzzle[i][j]) continue;
                DrawRectangle(j*screenHeight/N, i*screenWidth/N, screenHeight/N - 10, screenWidth/N - 10, LIGHTGRAY);
                DrawText(TextFormat("%d", puzzle[i][j]), j*screenHeight/N + screenHeight/(2*N) - 75/N, i*screenWidth/N + screenWidth/(2*N) - 75/N, 150/N, BLACK);
            }
        }

    EndDrawing();
}

void drawCongrats(){
    BeginDrawing();
    DrawRectangle(50, screenHeight/4, screenWidth - 100, screenHeight/4, GRAY);
    DrawText(TextFormat("CONGRATS YOU SOLVED THE PUZZLE"), 100, screenHeight/4 + 80, 30, GREEN);
    EndDrawing();
}

//Checks if move is valid (for creation)
bool isValidMove(Puzzle puzzle, int N, int move){
    //Find blank space
    int x, y;
    findBlank(puzzle, N, &x, &y);
    
    switch (move){
    //Move blank UP
    case 1:
        if (x == 0) return false;
        break;
    //Move blank DOWN
    case 2:
        if (x == N-1) return false;
        break;
    //Move blank RIGHT
    case 3:
        if (y == N-1) return false;
        break;
    //Move blank LEFT
    case 4:
        if (y == 0) return false;
        break;
    default:
        return false;
        break;
    }
    return true;
}

//Makes a move (for creation)
void makeMove(Puzzle puzzle, int N, int move){
    //Find blank space
    int x, y;
    findBlank(puzzle, N, &x, &y);
    
    switch (move){
    //Move blank UP
    case 1:
        swap(&puzzle[x][y], &puzzle[x-1][y]);
        break;
    //Move blank DOWN
    case 2:
        swap(&puzzle[x][y], &puzzle[x+1][y]);
        break;
    //Move blank RIGHT
    case 3:
        swap(&puzzle[x][y], &puzzle[x][y+1]);
        break;
    //Move blank LEFT
    case 4:
        swap(&puzzle[x][y], &puzzle[x][y-1]);
        break;
    default:
        break;
    }
}

//Randomizes puzzle
void randomizePuzzle(Puzzle puzzle, int N){
    int i, move;
    int total_moves = 200;
    for (i = 0; i < total_moves; i++){
        //Check if puzzle got randomized
        if (isSolution(puzzle, N) && i == total_moves - 1){
            i = 0;
        }

        //MOVE -> move zero -> move blank space
        //move == 1 -> UP
        //move == 2 -> DOWN
        //move == 3 -> RIGHT
        //move == 4 -> LEFT
        move = rand()%4 + 1; 

        if (isValidMove(puzzle, N, move)) makeMove(puzzle, N, move);

    }
}

//Checks if mouse is in a rectangle
bool isInRect(int mouse_x, int mouse_y, int i, int j, int N){
    return (mouse_x >= j*screenHeight/N && mouse_x <= (j+1)*screenHeight/N - 10 && mouse_y >= i*screenWidth/N && mouse_y <= (i+1)*screenWidth/N - 10);
}

//Finds the pressed piece
void findPressedPiece(Puzzle puzzle, int N, int mouse_x, int mouse_y, int *i, int *j){
    for (*i = 0; *i < N; (*i)++)
        for (*j = 0; *j < N; (*j)++)
            if (isInRect(mouse_x, mouse_y, *i, *j, N)) return; 
}

//Checks if player's move is valid
bool isValidSwap(Puzzle puzzle, int N, int i, int j, int zi, int zj){
    if (i == zi && j == zj) return false;
    if (i == N || j == N) return false;
    if (i == 0){
        if (j == 0){
            return (puzzle[i+1][j] == 0 || puzzle[i][j+1] == 0);
        } else if (j == N-1) {
            return (puzzle[i+1][j] == 0 || puzzle[i][j-1] == 0);
        } else {
            return (puzzle[i+1][j] == 0 || puzzle[i][j+1] == 0 || puzzle[i][j-1] == 0);
        }
    } else if (i == N-1){
        if (j == 0){
            return (puzzle[i-1][j] == 0 || puzzle[i][j+1] == 0);
        } else if (j == N-1) {
            return (puzzle[i-1][j] == 0 || puzzle[i][j-1] == 0);
        } else {
            return (puzzle[i-1][j] == 0 || puzzle[i][j+1] == 0 || puzzle[i][j-1] == 0);
        }
    } else {
        if (j == 0){
            return (puzzle[i+1][j] == 0 || puzzle[i-1][j] == 0 || puzzle[i][j+1] == 0);
        } else if (j == N-1) {
            return (puzzle[i+1][j] == 0 || puzzle[i-1][j] == 0 || puzzle[i][j-1] == 0);
        } else {
            return (puzzle[i+1][j] == 0 || puzzle[i-1][j] == 0 || puzzle[i][j+1] == 0 || puzzle[i][j-1] == 0);
        }
    }
}

//Game function
void playPuzzle(Puzzle puzzle, int N){
    //Game loop
    int i, j, zi, zj, mouse_x, mouse_y;
    do {
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)){
            mouse_x = GetMouseX();
            mouse_y = GetMouseY();
            findPressedPiece(puzzle, N, mouse_x, mouse_y, &i, &j);
            findBlank(puzzle, N, &zi, &zj);
            if (isValidSwap(puzzle, N, i, j, zi, zj)) swap(&puzzle[i][j], &puzzle[zi][zj]);
        }
	
        drawWindow(puzzle, N);
	    if (isSolution(puzzle, N)) break;
    } while (!WindowShouldClose());
    if (isSolution(puzzle, N)){
        drawCongrats();
        sleep(3);
    }
}

//Creates the puzzle
Puzzle createPuzzle(int N){
    int i, j;

    //Initialize puzzle array
    Puzzle puzzle = malloc(N * sizeof(unsigned *));
    checkMalloc(puzzle);
    for (i = 0; i < N; i++){
        puzzle[i] = malloc(N * sizeof(unsigned));
        checkMalloc(puzzle[i]);
    }

    //Initialize puzzle numbers, so when randomized it can be solved
    for (i = 0; i < N; i++)
        for (j = 0; j < N; j++)
            puzzle[i][j] = i*N + j;

    //Randomizing puzzle
    randomizePuzzle(puzzle, N);

    return puzzle;
}

//For freeing puzzle
void destroyPuzzle(Puzzle puzzle, int N){
    int i;
    for (i = 0; i < N; i++) free(puzzle[i]);
    free(puzzle);
}


int main(int argc, char **argv){
    //Check for the right arguments
    if (argc != 2){
        fprintf(stderr, "Usage: ./prog N\n");
        exit(1);
    }

    //Initializing randomizer
    srand(time(NULL));

    //Check if argument is valid
    int N = atoi(argv[1]);
    if (N <= 2){
        fprintf(stderr, "N must be greater than 2\n");
        exit(1); 
    }

    //Creating puzzle
    Puzzle puzzle = createPuzzle(N);

    //Initializing raylib
    InitWindow(screenWidth, screenHeight, "N-puzzle Window");
    SetTargetFPS(60);

    //Starting playfield
    playPuzzle(puzzle, N);

    //Freeing and destroying window
    destroyPuzzle(puzzle, N);
    CloseWindow();

    return 0;
}
