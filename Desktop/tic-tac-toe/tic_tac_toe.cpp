// ============================================================
//  TIC-TAC-TOE  |  Colorful console game in C++
// ============================================================
//  Features:
//    - Play vs Computer (Easy, Medium, Hard) or Two Players
//    - Hard level uses the minimax algorithm (it never loses)
//    - Hint (h) and Undo (u) while you play
//    - Colored board with the winning line highlighted
//    - Leaderboard saved in a file (scores.txt)
//
//  How to run:
//      g++ -std=c++11 tic_tac_toe.cpp -o tic_tac_toe
//      ./tic_tac_toe          (on Windows: .\tic_tac_toe)
// ============================================================

#ifdef _WIN32
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <unistd.h>
#endif

#include <algorithm>
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
using namespace std;


// ============================================================
//  1. LOOKS: colors and box-drawing characters
// ============================================================

// Color codes. Printing one changes the color of the text after it.
const string RESET    = "\033[0m";
const string BOLD     = "\033[1m";
const string RED      = "\033[91m";
const string GREEN    = "\033[92m";
const string YELLOW   = "\033[93m";
const string CYAN     = "\033[96m";
const string WHITE    = "\033[97m";
const string GRAY     = "\033[90m";
const string BG_GREEN = "\033[42m";   // green background (winning cells)

// Line-drawing characters (written as UTF-8 bytes so they work everywhere).
const string H_LINE    = "\xE2\x94\x80";   // horizontal line
const string V_LINE    = "\xE2\x94\x82";   // vertical line
const string TOP_LEFT  = "\xE2\x94\x8C";
const string TOP_MID   = "\xE2\x94\xAC";
const string TOP_RIGHT = "\xE2\x94\x90";
const string MID_LEFT  = "\xE2\x94\x9C";
const string CROSS     = "\xE2\x94\xBC";
const string MID_RIGHT = "\xE2\x94\xA4";
const string BOT_LEFT  = "\xE2\x94\x94";
const string BOT_MID   = "\xE2\x94\xB4";
const string BOT_RIGHT = "\xE2\x94\x98";

// Double lines for the title banner.
const string D_H  = "\xE2\x95\x90";
const string D_V  = "\xE2\x95\x91";
const string D_TL = "\xE2\x95\x94";
const string D_TR = "\xE2\x95\x97";
const string D_BL = "\xE2\x95\x9A";
const string D_BR = "\xE2\x95\x9D";

const string INDENT = "    ";   // left margin for everything we print


// ============================================================
//  2. GAME DATA
// ============================================================

// The board is a string of 9 characters: 'X', 'O' or ' ' (empty).
// Cells are numbered 0-8 inside the program, but shown as 1-9 to players:
//      1 | 2 | 3
//      4 | 5 | 6
//      7 | 8 | 9
typedef string Board;

// The 8 ways to win: 3 rows, 3 columns, 2 diagonals.
const int WIN_LINES[8][3] = {
    {0, 1, 2}, {3, 4, 5}, {6, 7, 8},
    {0, 3, 6}, {1, 4, 7}, {2, 5, 8},
    {0, 4, 8}, {2, 4, 6}
};

// Everything about the match that is being played.
struct Game {
    Board       board;
    vector<int> history;      // cells played so far (used by Undo)
    bool        vsComputer;
    int         level;        // 1 = Easy, 2 = Medium, 3 = Hard
    string      nameX;
    string      nameO;
    int         scoreX;
    int         scoreO;
    int         draws;
    char        turn;         // whose turn it is: 'X' or 'O'
    char        starter;      // who starts the NEXT round (alternates)
};

// One line of the leaderboard file.
struct Record {
    string name;
    int    wins;
    int    losses;
    int    draws;
};


// ============================================================
//  3. SMALL HELPER FUNCTIONS
// ============================================================

// Turns on colors and special characters in the Windows console.
void setupTerminal() {
#ifdef _WIN32
    SetConsoleOutputCP(65001);   // UTF-8 text
    HANDLE console = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD mode = 0;
    if (GetConsoleMode(console, &mode)) {
        SetConsoleMode(console, mode | 0x0004);   // enable color codes
    }
#endif
}

void clearScreen() {
    cout << "\033[H\033[2J\033[3J";
}

void pauseMs(int milliseconds) {
    cout << flush;
#ifdef _WIN32
    Sleep(milliseconds);
#else
    usleep(milliseconds * 1000);
#endif
}

// Shows a question and returns what the user typed.
string readLine(const string& prompt) {
    string line;
    cout << prompt << flush;
    if (!getline(cin, line)) {
        cout << "\n";
        exit(0);   // input was closed, stop the program
    }
    return line;
}

string toText(int number) {
    ostringstream stream;
    stream << number;
    return stream.str();
}

string paint(const string& text, const string& style) {
    return style + text + RESET;
}

string repeatText(const string& text, int times) {
    string result;
    for (int i = 0; i < times; i++) {
        result += text;
    }
    return result;
}

string padRight(string text, size_t width) {
    while (text.size() < width) text += ' ';
    return text;
}

string padCenter(const string& text, size_t width) {
    size_t space = (width > text.size()) ? width - text.size() : 0;
    size_t left  = space / 2;
    return string(left, ' ') + text + string(space - left, ' ');
}

// Makes a safe player name: printable letters only, max 12 characters.
string cleanName(const string& name, const string& fallback) {
    string result;
    for (size_t i = 0; i < name.size(); i++) {
        char c = name[i];
        if (c >= 32 && c < 127 && c != '|') {
            result += c;
        }
    }
    size_t start = result.find_first_not_of(' ');
    if (start == string::npos) {
        return fallback;
    }
    result = result.substr(start);
    if (result.size() > 12) {
        result.resize(12);
    }
    return result;
}

char otherPlayer(char player) {
    return (player == 'X') ? 'O' : 'X';
}

string levelName(int level) {
    if (level == 1) return "Easy";
    if (level == 2) return "Medium";
    return "Hard";
}


// ============================================================
//  4. BOARD RULES
// ============================================================

bool boardFull(const Board& board) {
    return board.find(' ') == string::npos;
}

// Returns 'X' or 'O' if someone has three in a line, otherwise ' '.
// The three winning cells are stored in 'line'.
char findWinner(const Board& board, int line[3]) {
    for (int i = 0; i < 8; i++) {
        int a = WIN_LINES[i][0];
        int b = WIN_LINES[i][1];
        int c = WIN_LINES[i][2];

        if (board[a] != ' ' && board[a] == board[b] && board[a] == board[c]) {
            line[0] = a;
            line[1] = b;
            line[2] = c;
            return board[a];
        }
    }
    line[0] = line[1] = line[2] = -1;
    return ' ';
}

vector<int> freeCells(const Board& board) {
    vector<int> cells;
    for (int i = 0; i < 9; i++) {
        if (board[i] == ' ') {
            cells.push_back(i);
        }
    }
    return cells;
}


// ============================================================
//  5. COMPUTER PLAYER
// ============================================================

int pickRandom(const vector<int>& options) {
    return options[rand() % options.size()];
}

// Looks for a cell where 'player' would win on the next move (-1 if none).
int findWinningMove(Board board, char player) {
    int line[3];
    for (int cell = 0; cell < 9; cell++) {
        if (board[cell] == ' ') {
            board[cell] = player;
            bool wins = (findWinner(board, line) == player);
            board[cell] = ' ';
            if (wins) return cell;
        }
    }
    return -1;
}

// Minimax: try every possible move and reply until the game ends.
//   computer wins -> positive score (sooner is better)
//   computer loses -> negative score (later is better)
//   draw -> 0
int minimax(Board& board, char me, char turn, int depth) {
    int line[3];
    char winner = findWinner(board, line);

    if (winner == me)  return 10 - depth;
    if (winner != ' ') return depth - 10;
    if (boardFull(board)) return 0;

    bool myTurn = (turn == me);
    int best = myTurn ? -100 : 100;

    for (int cell = 0; cell < 9; cell++) {
        if (board[cell] == ' ') {
            board[cell] = turn;
            int score = minimax(board, me, otherPlayer(turn), depth + 1);
            board[cell] = ' ';

            if (myTurn) best = max(best, score);
            else        best = min(best, score);
        }
    }
    return best;
}

// The best possible move for 'me' (used by Hard level and by Hint).
int bestMove(Board board, char me) {
    // First move of the game: any corner or the center is equally good.
    if (freeCells(board).size() == 9) {
        int starts[5] = {0, 2, 4, 6, 8};
        return starts[rand() % 5];
    }

    int bestScore = -100;
    vector<int> bestCells;

    for (int cell = 0; cell < 9; cell++) {
        if (board[cell] == ' ') {
            board[cell] = me;
            int score = minimax(board, me, otherPlayer(me), 1);
            board[cell] = ' ';

            if (score > bestScore) {
                bestScore = score;
                bestCells.clear();
            }
            if (score == bestScore) {
                bestCells.push_back(cell);
            }
        }
    }
    return pickRandom(bestCells);
}

int mediumMove(const Board& board, char me) {
    int cell = findWinningMove(board, me);               // 1. win if possible
    if (cell != -1) return cell;

    cell = findWinningMove(board, otherPlayer(me));      // 2. block the opponent
    if (cell != -1) return cell;

    if (board[4] == ' ') return 4;                       // 3. take the center

    return pickRandom(freeCells(board));                 // 4. anything else
}

int computerMove(const Board& board, char me, int level) {
    if (level == 1) return pickRandom(freeCells(board));
    if (level == 2) return mediumMove(board, me);
    return bestMove(board, me);
}


// ============================================================
//  6. DRAWING THE SCREEN
// ============================================================

void drawBanner() {
    string line = repeatText(D_H, 23);
    cout << "\n";
    cout << INDENT << paint(D_TL + line + D_TR, CYAN) << "\n";
    cout << INDENT << paint(D_V, CYAN)
         << paint(padCenter("TIC-TAC-TOE", 23), BOLD + WHITE)
         << paint(D_V, CYAN) << "\n";
    cout << INDENT << paint(D_BL + line + D_BR, CYAN) << "\n\n";
}

// One text line (0, 1 or 2) of a 7-character-wide cell.
string cellLine(const Board& board, int cell, int row, bool highlight) {
    string background = highlight ? BG_GREEN : "";

    if (row != 1) {
        return paint("       ", background);   // top and bottom of the cell
    }

    string symbol(1, board[cell]);
    string style;
    if (board[cell] == 'X')      style = BOLD + RED;
    else if (board[cell] == 'O') style = BOLD + CYAN;
    else {
        symbol = string(1, static_cast<char>('1' + cell));   // show the cell number
        style  = GRAY;
    }
    if (highlight) {
        style = BOLD + WHITE;
    }
    return paint("   ", background) + paint(symbol, background + style) + paint("   ", background);
}

string borderLine(const string& left, const string& middle, const string& right) {
    string part = repeatText(H_LINE, 7);
    return INDENT + paint(left + part + middle + part + middle + part + right, GRAY);
}

void drawBoard(const Board& board, const int winLine[3]) {
    string wall = paint(V_LINE, GRAY);

    cout << borderLine(TOP_LEFT, TOP_MID, TOP_RIGHT) << "\n";

    for (int row = 0; row < 3; row++) {
        for (int part = 0; part < 3; part++) {
            cout << INDENT << wall;
            for (int col = 0; col < 3; col++) {
                int cell = row * 3 + col;
                bool highlight = (cell == winLine[0] || cell == winLine[1] || cell == winLine[2]);
                cout << cellLine(board, cell, part, highlight) << wall;
            }
            cout << "\n";
        }
        if (row < 2) {
            cout << borderLine(MID_LEFT, CROSS, MID_RIGHT) << "\n";
        }
    }
    cout << borderLine(BOT_LEFT, BOT_MID, BOT_RIGHT) << "\n";
}

void drawScore(const Game& game) {
    cout << INDENT << paint("X", BOLD + RED) << "  " << padRight(game.nameX, 14)
         << paint(toText(game.scoreX), BOLD + WHITE) << "\n";
    cout << INDENT << paint("O", BOLD + CYAN) << "  " << padRight(game.nameO, 14)
         << paint(toText(game.scoreO), BOLD + WHITE) << "\n";
    cout << INDENT << paint("=", GRAY) << "  " << padRight("Draws", 14)
         << paint(toText(game.draws), BOLD + WHITE) << "\n";

    if (game.vsComputer) {
        cout << INDENT << paint("Level: " + levelName(game.level), GRAY) << "\n";
    }
    cout << "\n";
}


// ============================================================
//  7. LEADERBOARD (saved in scores.txt)
// ============================================================
//  Each line of the file looks like:   name|wins|losses|draws

vector<Record> loadRecords() {
    vector<Record> records;
    ifstream file("scores.txt");
    string line;

    while (getline(file, line)) {
        istringstream parts(line);
        string name, wins, losses, draws;

        if (getline(parts, name, '|') && getline(parts, wins, '|') &&
            getline(parts, losses, '|') && getline(parts, draws, '|')) {
            Record record;
            record.name   = name;
            record.wins   = atoi(wins.c_str());
            record.losses = atoi(losses.c_str());
            record.draws  = atoi(draws.c_str());
            records.push_back(record);
        }
    }
    return records;
}

void saveRecords(const vector<Record>& records) {
    ofstream file("scores.txt");
    for (size_t i = 0; i < records.size(); i++) {
        file << records[i].name << '|' << records[i].wins << '|'
             << records[i].losses << '|' << records[i].draws << '\n';
    }
}

// outcome: 'W' = win, 'L' = loss, 'D' = draw
void addResult(vector<Record>& records, const string& name, char outcome) {
    size_t index = records.size();
    for (size_t i = 0; i < records.size(); i++) {
        if (records[i].name == name) index = i;
    }
    if (index == records.size()) {
        Record fresh;
        fresh.name = name;
        fresh.wins = fresh.losses = fresh.draws = 0;
        records.push_back(fresh);
    }
    if (outcome == 'W') records[index].wins++;
    if (outcome == 'L') records[index].losses++;
    if (outcome == 'D') records[index].draws++;
}

// Saves the result of one finished round for every human player.
void saveRoundResult(const Game& game, char winner) {
    vector<Record> records = loadRecords();

    addResult(records, game.nameX, winner == 'X' ? 'W' : (winner == 'O' ? 'L' : 'D'));
    if (!game.vsComputer) {
        addResult(records, game.nameO, winner == 'O' ? 'W' : (winner == 'X' ? 'L' : 'D'));
    }
    saveRecords(records);
}

bool betterRecord(const Record& a, const Record& b) {
    if (a.wins   != b.wins)   return a.wins > b.wins;
    if (a.losses != b.losses) return a.losses < b.losses;
    return a.name < b.name;
}

void showLeaderboard() {
    vector<Record> records = loadRecords();
    sort(records.begin(), records.end(), betterRecord);

    clearScreen();
    drawBanner();
    cout << INDENT << paint("LEADERBOARD", BOLD + YELLOW) << "\n\n";

    if (records.empty()) {
        cout << INDENT << "No games saved yet. Play a round first!\n";
    } else {
        cout << INDENT << paint("#   " + padRight("Player", 14) + "W    L    D", GRAY) << "\n";
        for (size_t i = 0; i < records.size() && i < 10; i++) {
            string rankColor = (i == 0) ? BOLD + YELLOW : WHITE;
            cout << INDENT
                 << paint(padRight(toText(static_cast<int>(i) + 1), 4) + padRight(records[i].name, 14), rankColor)
                 << paint(padRight(toText(records[i].wins), 5), GREEN)
                 << paint(padRight(toText(records[i].losses), 5), RED)
                 << paint(toText(records[i].draws), GRAY) << "\n";
        }
    }
    readLine("\n" + INDENT + "Press Enter to go back...");
}


// ============================================================
//  8. PLAYING A ROUND
// ============================================================

bool isComputerTurn(const Game& game) {
    return game.vsComputer && game.turn == 'O';
}

// Undo is possible when the human has played at least one move.
bool canUndo(const Game& game) {
    if (!game.vsComputer) {
        return !game.history.empty();
    }
    for (size_t i = 0; i < game.history.size(); i++) {
        if (game.board[game.history[i]] == 'X') return true;
    }
    return false;
}

// Takes back the last move. Against the computer, its reply is taken back too.
void undoMove(Game& game) {
    do {
        int cell = game.history.back();
        game.history.pop_back();
        game.turn = game.board[cell];
        game.board[cell] = ' ';
    } while (game.vsComputer && game.turn == 'O');
}

void startNewRound(Game& game) {
    game.board   = "         ";
    game.history.clear();
    game.turn    = game.starter;
    game.starter = otherPlayer(game.starter);   // the other player starts next time
}

// Plays one round. Returns false if the player chose to quit.
bool playRound(Game& game) {
    string message;

    while (true) {
        int winLine[3];
        char winner = findWinner(game.board, winLine);
        bool over   = (winner != ' ' || boardFull(game.board));

        clearScreen();
        drawBanner();
        drawScore(game);
        drawBoard(game.board, winLine);
        cout << "\n";

        // ---- The round has ended ----
        if (over) {
            if (winner == 'X') {
                game.scoreX++;
                cout << INDENT << paint(game.nameX + " wins!", BOLD + GREEN) << "\n";
            } else if (winner == 'O') {
                game.scoreO++;
                cout << INDENT << paint(game.nameO + " wins!", BOLD + GREEN) << "\n";
            } else {
                game.draws++;
                cout << INDENT << paint("It's a draw!", BOLD + YELLOW) << "\n";
            }
            saveRoundResult(game, winner);
            return true;
        }

        // ---- Computer's turn ----
        if (isComputerTurn(game)) {
            cout << INDENT << paint("Computer is thinking...", GRAY) << "\n";
            pauseMs(700);

            int cell = computerMove(game.board, 'O', game.level);
            game.board[cell] = 'O';
            game.history.push_back(cell);
            game.turn = 'X';
            message = "Computer chose cell " + toText(cell + 1) + ".";
            continue;
        }

        // ---- Human's turn ----
        string name  = (game.turn == 'X') ? game.nameX : game.nameO;
        string color = (game.turn == 'X') ? BOLD + RED : BOLD + CYAN;

        if (!message.empty()) {
            cout << INDENT << paint(message, YELLOW) << "\n";
            message = "";
        }
        cout << INDENT << paint(name + "'s turn (" + string(1, game.turn) + ")", color) << "\n";
        cout << INDENT << paint("1-9 = play   u = undo   h = hint   q = quit", GRAY) << "\n";

        string input = readLine(INDENT + "> ");

        if (input == "q" || input == "Q") {
            return false;
        } else if (input == "u" || input == "U") {
            if (canUndo(game)) {
                undoMove(game);
                message = "Move undone.";
            } else {
                message = "Nothing to undo yet.";
            }
        } else if (input == "h" || input == "H") {
            message = "Hint: try cell " + toText(bestMove(game.board, game.turn) + 1) + ".";
        } else if (input.size() == 1 && input[0] >= '1' && input[0] <= '9') {
            int cell = input[0] - '1';
            if (game.board[cell] == ' ') {
                game.board[cell] = game.turn;
                game.history.push_back(cell);
                game.turn = otherPlayer(game.turn);
            } else {
                message = "Cell " + input + " is already taken.";
            }
        } else {
            message = "Please type a number from 1 to 9 (or u, h, q).";
        }
    }
}


// ============================================================
//  9. MENUS AND MAIN PROGRAM
// ============================================================

void playMatch(bool vsComputer) {
    Game game;
    game.vsComputer = vsComputer;
    game.level      = 1;
    game.scoreX = game.scoreO = game.draws = 0;
    game.starter = 'X';

    clearScreen();
    drawBanner();
    game.nameX = cleanName(readLine(INDENT + (vsComputer ? "Your name: " : "Player X name: ")),
                           vsComputer ? "Player" : "Player X");
    if (vsComputer) {
        game.nameO = "Computer";
        cout << "\n" << INDENT << paint("1", YELLOW) << "  Easy\n"
             << INDENT << paint("2", YELLOW) << "  Medium\n"
             << INDENT << paint("3", YELLOW) << "  Hard (unbeatable)\n";
        int choice = atoi(readLine(INDENT + "Choose difficulty (1-3): ").c_str());
        game.level = (choice == 2 || choice == 3) ? choice : 1;
    } else {
        game.nameO = cleanName(readLine(INDENT + "Player O name: "), "Player O");
    }

    bool keepPlaying = true;
    while (keepPlaying) {
        startNewRound(game);
        keepPlaying = playRound(game);

        if (keepPlaying) {
            string again = readLine("\n" + INDENT + "Play another round? (y/n): ");
            keepPlaying = (again == "y" || again == "Y");
        }
    }
}

void showHowToPlay() {
    clearScreen();
    drawBanner();
    cout << INDENT << paint("HOW TO PLAY", BOLD + YELLOW) << "\n\n"
         << INDENT << "Get 3 of your symbols in a row, column or diagonal.\n"
         << INDENT << "Type the number of the cell you want (1-9).\n\n"
         << INDENT << paint("u", YELLOW) << "  take back your last move\n"
         << INDENT << paint("h", YELLOW) << "  get a hint for the best move\n"
         << INDENT << paint("q", YELLOW) << "  quit the current match\n\n"
         << INDENT << "X always starts the first round. After that,\n"
         << INDENT << "the player who starts alternates each round.\n";
    readLine("\n" + INDENT + "Press Enter to go back...");
}

int showMenu() {
    clearScreen();
    drawBanner();
    cout << INDENT << paint("1", YELLOW) << "  Play vs Computer\n"
         << INDENT << paint("2", YELLOW) << "  Two Players\n"
         << INDENT << paint("3", YELLOW) << "  Leaderboard\n"
         << INDENT << paint("4", YELLOW) << "  How to Play\n"
         << INDENT << paint("5", YELLOW) << "  Quit\n\n";
    return atoi(readLine(INDENT + "Choose (1-5): ").c_str());
}

int main() {
    setupTerminal();
    srand(static_cast<unsigned>(time(0)));

    bool running = true;
    while (running) {
        switch (showMenu()) {
            case 1:  playMatch(true);    break;
            case 2:  playMatch(false);   break;
            case 3:  showLeaderboard();  break;
            case 4:  showHowToPlay();    break;
            case 5:  running = false;    break;
            default: break;   // invalid choice: show the menu again
        }
    }

    clearScreen();
    cout << "\n" << INDENT << "Thanks for playing. Goodbye!\n\n";
    return 0;
}
