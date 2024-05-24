#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <windowsx.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>

struct pos
{
    int x;
    int y;
};

const int DURATION = 120;

#define GRID_DIM 4
#define CELL_COUNT (GRID_DIM * GRID_DIM)

const int TOP_TEXT_HEIGHT = 48;
const int CELL_DIM = 80;
const int PADDING_SIZE = 16;
const int WORD_VIEW_HEIGHT = 64;
const int WORD_VIEW_WIDTH = (CELL_DIM * GRID_DIM + PADDING_SIZE * (GRID_DIM - 1));
const int ROUNDNESS = 25;

const int TOP_TEXT_Y = PADDING_SIZE;
const int WORD_VIEW_Y = TOP_TEXT_Y + TOP_TEXT_HEIGHT + PADDING_SIZE;
const int GRID_Y = WORD_VIEW_Y + WORD_VIEW_HEIGHT + PADDING_SIZE;

const COLORREF BLACK = RGB(0, 0, 0);
const COLORREF WHITE = RGB(0xff, 0xff, 0xff);
const COLORREF ORANGE = RGB(0xff, 0xa6, 0x00);

const COLORREF BACKGROUND_COLOUR = RGB(0x01, 0x70, 0xc1);
const COLORREF CELL_COLOUR = WHITE;

const int WIDTH = CELL_DIM * GRID_DIM + PADDING_SIZE * (GRID_DIM + 1);
const int HEIGHT = PADDING_SIZE + TOP_TEXT_HEIGHT + PADDING_SIZE + WORD_VIEW_HEIGHT + WIDTH;

const int ADJACENT[4] = {-1, 1, -GRID_DIM, GRID_DIM};

SOCKET sock;

struct pos cell_pos[CELL_COUNT];

char board[CELL_COUNT];
int time_left = DURATION;
int score = 0;
bool high_cells[CELL_COUNT];
int prev_high = -1;
char word[CELL_COUNT];
int word_length = 0;
bool word_correct = false;
bool game_over = false;

void highlight_cell(int i)
{
    high_cells[i] = true;
    prev_high = i;
    word[word_length] = board[i];
    word_length += 1;
}

LRESULT CALLBACK WindowProc(HWND wnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
    switch (msg)
    {
    case WM_MOUSEMOVE:
    {
        if (GetAsyncKeyState(VK_LBUTTON) >= 0)
        {
            return 0;
        }

        POINT pt = {GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};

        HRGN hrgn;

        for (int i = 0; i < CELL_COUNT; i++)
        {
            hrgn = CreateRoundRectRgn(cell_pos[i].x, cell_pos[i].y, cell_pos[i].x + CELL_DIM, cell_pos[i].y + CELL_DIM, ROUNDNESS, ROUNDNESS);
            if (PtInRegion(hrgn, pt.x, pt.y) && high_cells[i] == false)
            {
                for (int j = 0; j < GRID_DIM; ++j)
                {
                    int adj_i = i + ADJACENT[j];

                    if (prev_high == adj_i)
                    {
                        highlight_cell(i);
                        InvalidateRect(wnd, 0, TRUE);

                        break;
                    }
                }

                break;
            }
            DeleteObject(hrgn);
        }
    }
        return 0;

    case WM_LBUTTONDOWN:
    {
        POINT pt = {GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};

        HRGN hrgn;

        for (int i = 0; i < CELL_COUNT; i++)
        {
            hrgn = CreateRoundRectRgn(cell_pos[i].x, cell_pos[i].y, cell_pos[i].x + CELL_DIM, cell_pos[i].y + CELL_DIM, 25, 25);
            if (PtInRegion(hrgn, pt.x, pt.y))
            {
                highlight_cell(i);
                InvalidateRect(wnd, 0, TRUE);

                break;
            }
            DeleteObject(hrgn);
        }

        InvalidateRect(wnd, 0, TRUE);
    }
        return 0;

    case WM_LBUTTONUP:
    {
        if (word_length > 0)
        {
            int result;

            result = send(sock, (char *)&word_length, sizeof(word_length), 0);
            result = send(sock, word, word_length, 0);

            char byte;
            result = recv(sock, &byte, 1, 0);

            switch (byte)
            {
            case 0:
                word_correct = false;
                break;
            case 1:
                word_correct = true;
                score += word_length;
                break;
            }

            ZeroMemory(high_cells, CELL_COUNT);
            prev_high = -1;
            ZeroMemory(word, CELL_COUNT);
            word_length = 0;

            InvalidateRect(wnd, 0, TRUE);
        }
    }
        return 0;

    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(wnd, &ps);

        HBRUSH bg_brush = CreateSolidBrush(BACKGROUND_COLOUR);

        FillRect(dc, &ps.rcPaint, bg_brush);

        HBRUSH white_brush = CreateSolidBrush(WHITE);
        SelectObject(dc, white_brush);

        HBRUSH orange_brush = CreateSolidBrush(ORANGE);

        HPEN pen = CreatePen(PS_NULL, 0, WHITE);

        SelectObject(dc, pen);

        SetBkMode(dc, TRANSPARENT);

        LOGFONT lf;
        ZeroMemory(&lf, sizeof(LOGFONT));
        strcpy(lf.lfFaceName, "Helvetica");
        lf.lfWeight = 650;

        SetTextColor(dc, WHITE);

        lf.lfHeight = TOP_TEXT_HEIGHT;
        HFONT top_text_font = CreateFontIndirect(&lf);

        SelectObject(dc, top_text_font);

        char timer[5];
        sprintf(timer, "%02d:%02d", time_left / 60, time_left % 60);

        TextOut(dc, PADDING_SIZE, PADDING_SIZE, timer, strlen(timer));

        char score_str[4];
        ZeroMemory(score_str, 4);
        itoa(score, score_str, 10);

        SIZE text_size;
        GetTextExtentPoint32(dc, score_str, strlen(score_str), &text_size);

        TextOut(dc, WIDTH - PADDING_SIZE - text_size.cx, TOP_TEXT_Y, score_str, strlen(score_str));

        DeleteObject(top_text_font);

        SetTextColor(dc, BLACK);

        RoundRect(dc, PADDING_SIZE, WORD_VIEW_Y, WORD_VIEW_WIDTH + PADDING_SIZE, WORD_VIEW_HEIGHT + WORD_VIEW_Y, ROUNDNESS, ROUNDNESS);

        if (word_length > 0)
        {
            lf.lfHeight = 36;
            HFONT word_view_font = CreateFontIndirect(&lf);

            SelectObject(dc, word_view_font);

            char word[word_length];
            ZeroMemory(word, word_length);
            strcpy(word, word);

            GetTextExtentPoint32(dc, word, strlen(word), &text_size);

            TextOut(dc, PADDING_SIZE + WORD_VIEW_WIDTH / 2 - text_size.cx / 2, WORD_VIEW_Y + WORD_VIEW_HEIGHT / 2 - text_size.cy / 2, word, word_length);

            DeleteObject(word_view_font);
        }

        lf.lfHeight = 54;
        HFONT cell_font = CreateFontIndirect(&lf);

        SelectObject(dc, cell_font);

        for (int i = 0; i < CELL_COUNT; ++i)
        {
            if (high_cells[i])
            {
                SelectObject(dc, orange_brush);
            }

            RoundRect(
                dc,
                cell_pos[i].x,
                cell_pos[i].y,
                cell_pos[i].x + CELL_DIM,
                cell_pos[i].y + CELL_DIM,
                ROUNDNESS, ROUNDNESS);

            GetTextExtentPoint32(dc, &board[i], 1, &text_size);

            TextOut(dc, cell_pos[i].x + CELL_DIM / 2 - text_size.cx / 2, cell_pos[i].y + CELL_DIM / 2 - text_size.cy / 2, &board[i], 1);

            if (high_cells[i])
            {
                SelectObject(dc, white_brush);
            }
        }

        DeleteObject(bg_brush);
        DeleteObject(white_brush);
        DeleteObject(orange_brush);
        DeleteObject(pen);
        DeleteObject(cell_font);

        EndPaint(wnd, &ps);
    }
        return 0;

    case WM_TIMER:
    {
        time_left -= 1;

        if (time_left == 0)
        {
            KillTimer(wnd, 1);

            int opponent_score;
            int result = recv(sock, (char *)&opponent_score, sizeof(opponent_score), 0);

            char message[50];
            sprintf(message, "Your score: %d\nOpponent score: %d", score, opponent_score);
            MessageBox(NULL, message, "Score", MB_OK);

            DestroyWindow(wnd);
        }

        InvalidateRect(wnd, NULL, TRUE);
    }
        return 0;

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(wnd, msg, wparam, lparam);
}

int WINAPI WinMain(HINSTANCE instance, HINSTANCE prev_instance, LPSTR cmd_line, int cmd_show)
{
    char *node_name = "localhost";
    char *service_name = "27015";

    int argc = __argc;
    char **argv = __argv;

    if (argc > 1)
        node_name = argv[1];

    if (argc > 2)
        service_name = argv[2];

    // Connessione al server
    int result;

    WSADATA wsa_data;
    result = WSAStartup(MAKEWORD(2, 2), &wsa_data);
    if (result != 0)
    {
        printf("WSAStartup failed with error: %d\n", result);
        return 1;
    }

    struct addrinfo hints;
    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    struct addrinfo *addr_info = NULL;

    result = getaddrinfo(node_name, service_name, &hints, &addr_info);
    if (result != 0)
    {
        printf("getaddrinfo failed with error: %d\n", result);
        WSACleanup();
        return 1;
    }

    sock = INVALID_SOCKET;
    for (struct addrinfo *curr_addr_info = addr_info; curr_addr_info != NULL; curr_addr_info = curr_addr_info->ai_next)
    {
        sock = socket(curr_addr_info->ai_family, curr_addr_info->ai_socktype,
                      curr_addr_info->ai_protocol);
        if (sock == INVALID_SOCKET)
        {
            printf("socket failed with error: %d\n", WSAGetLastError());
            WSACleanup();
            return 1;
        }

        result = connect(sock, curr_addr_info->ai_addr, (int)curr_addr_info->ai_addrlen);
        if (result == SOCKET_ERROR)
        {
            closesocket(sock);
            sock = INVALID_SOCKET;
            continue;
        }
        break;
    }

    freeaddrinfo(addr_info);

    if (sock == INVALID_SOCKET)
    {
        printf("Unable to connect to server!\n");
        WSACleanup();
        return 1;
    }

    result = recv(sock, board, CELL_COUNT, 0);

    int x = PADDING_SIZE;
    int y = GRID_Y;

    for (int i = 0; i < GRID_DIM; ++i)
    {
        for (int j = 0; j < GRID_DIM; ++j)
        {
            cell_pos[i * GRID_DIM + j].x = x;
            cell_pos[i * GRID_DIM + j].y = y;

            x += CELL_DIM + PADDING_SIZE;
        }

        x = PADDING_SIZE;
        y += CELL_DIM + PADDING_SIZE;
    }

    // Creazione della finestra
    const char CLASS_NAME[] = "Ruzzle";

    WNDCLASS wc = {};

    wc.lpfnWndProc = WindowProc;
    wc.hInstance = instance;
    wc.lpszClassName = CLASS_NAME;

    RegisterClass(&wc);

    DWORD dwStyle = (WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX);

    RECT windowRect = {0, 0, WIDTH, HEIGHT};
    AdjustWindowRectEx(&windowRect, dwStyle, FALSE, 0);

    HWND wnd = CreateWindowEx(
        0,
        CLASS_NAME,
        "Ruzzle",
        dwStyle,
        GetSystemMetrics(SM_CXSCREEN) / 2 - 500 / 2,
        GetSystemMetrics(SM_CYSCREEN) / 2 - 500 / 2,
        windowRect.right - windowRect.left,
        windowRect.bottom - windowRect.top,
        NULL,
        NULL,
        instance,
        NULL);

    ShowWindow(wnd, cmd_show);

    char buf[1];
    result = send(sock, buf, 1, 0);

    result = recv(sock, buf, 1, 0);

    SetTimer(wnd, 1, 1000, NULL);

    MSG msg = {};
    while (GetMessage(&msg, NULL, 0, 0) > 0)
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    result = shutdown(sock, SD_SEND);
    if (result == SOCKET_ERROR)
    {
        printf("shutdown failed with error: %d\n", WSAGetLastError());
        closesocket(sock);
        WSACleanup();
        return 1;
    }

    closesocket(sock);
    WSACleanup();
    return 0;
}
