#include <SDL3/SDL.h>
#include <fcntl.h>
#include <pty.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <utmp.h>

extern char* environ[];

int main() {
    int master;
    int slave;

    openpty(&master, &slave, NULL, NULL, NULL);
    fcntl(master, F_SETFL, O_NONBLOCK);

    pid_t pid = fork();
    if (pid == 0) {
        login_tty(slave);

        // Reverte para dumb para evitar sequências complexas que quebram a renderização
        setenv("TERM", "dumb", 1);

        char* shell = getenv("SHELL");
        shell = NULL;
        if (!shell) shell = "/bin/sh";

        execl(shell, shell, "-i", NULL);

        _exit(1);
    }

    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS);
    SDL_Window* window = SDL_CreateWindow("Terminal", 800, 600, SDL_WINDOW_RESIZABLE);
    SDL_Renderer* rend = SDL_CreateRenderer(window, NULL);
    SDL_StartTextInput(window);
    SDL_Event event;

    static char display_buffer[16384] = {0};
    int display_idx = 0;
    int in_esc = 0;

    for (;;) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) return 0;

            if (event.type == SDL_EVENT_KEY_DOWN) {
                if (event.key.key == SDLK_ESCAPE) return 0;
                if (event.key.key == SDLK_RETURN) write(master, "\n", 1);
                if (event.key.key == SDLK_BACKSPACE) write(master, "\b", 1);
                if (event.key.key == SDLK_TAB) write(master, "\t", 1);
                if (event.key.key == SDLK_L && (event.key.mod & SDL_KMOD_CTRL)) {
                    write(master, "\014", 1);
                }
            }

            if (event.type == SDL_EVENT_TEXT_INPUT) {
                write(master, event.text.text, SDL_strlen(event.text.text));
            }
        }

        char read_buffer[1024];
        int n = read(master, read_buffer, sizeof(read_buffer));
        if (n > 0) {
            for (int i = 0; i < n; i++) {
                unsigned char c = read_buffer[i];

                // Filtro ANSI aprimorado
                if (c == 27) {
                    in_esc = 1;
                    continue;
                }
                if (in_esc) {
                    // Termina a sequência em caracteres de comando (A-Z, a-z)
                    if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z')) {
                        // Se for comando de limpar tela (J) ou ir para topo (H)
                        if (c == 'J' || c == 'H') {
                            display_idx = 0;
                            display_buffer[0] = '\0';
                        }
                        in_esc = 0;
                    }
                    continue;
                }

                if (c == '\014') {  // Form Feed (Ctrl+L)
                    display_idx = 0;
                    display_buffer[0] = '\0';
                    continue;
                }

                if (c == '\r') continue;
                if (c == '\b' || c == 127) {
                    if (display_idx > 0) display_idx--;
                } else if (c >= 32 || c == '\n' || c == '\t') {
                    if (display_idx < (int)sizeof(display_buffer) - 1) {
                        display_buffer[display_idx++] = c;
                    }
                }
                display_buffer[display_idx] = '\0';
            }
        }

        SDL_SetRenderDrawColor(rend, 0, 0, 0, 255);
        SDL_RenderClear(rend);
        SDL_SetRenderDrawColor(rend, 255, 255, 255, 255);

        int win_w, win_h;
        SDL_GetWindowSize(window, &win_w, &win_h);
        int max_lines = (win_h - 20) / 16;

        // Pula apenas as quebras de linha que sobraram de um clear no início
        char* line = display_buffer;
        while (*line == '\n' && display_idx > 0) line++;

        int total_lines = 0;
        for (char* p = line; *p; p++)
            if (*p == '\n') total_lines++;

        if (total_lines > max_lines) {
            int lines_to_skip = total_lines - max_lines;
            while (lines_to_skip > 0 && *line) {
                if (*line == '\n') lines_to_skip--;
                line++;
            }
        }

        float y = 10;
        while (line && *line) {
            char* next_line = strchr(line, '\n');
            if (next_line) *next_line = '\0';
            SDL_RenderDebugText(rend, 10, y, line);
            y += 16;
            if (next_line) {
                *next_line = '\n';
                line = next_line + 1;
            } else
                break;
        }

        SDL_RenderPresent(rend);
        SDL_Delay(16);
    }

    SDL_DestroyWindow(window);
    SDL_DestroyRenderer(rend);
    SDL_Quit();

    return 0;
}
