// made with love and also with Windows compatibility in mind
// create an issue if it doesnt work
#include <stdio.h>
#include <stdlib.h>
// #include <process.h>
#include <string.h>
// #include <conio.h>
#include <math.h>

#if defined(_WIN32) || defined(_WIN64)
    #include <conio.h>
    #define getch() _getch()
#else
    #include <termios.h>
    #include <unistd.h>
    int getch(void) {
        struct termios oldt, newt;
        int ch;

        tcgetattr(STDIN_FILENO, &oldt);
        newt = oldt;
        newt.c_lflag &= ~(ICANON | ECHO);
        tcsetattr(STDIN_FILENO, TCSANOW, &newt);

        ch = getchar();

        tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
        return ch;
    }
#endif

#if defined(_WIN32) || defined(_WIN64)
    #include <process.h>
    #define _P_WAIT 0
    #define _P_NOWAIT 1
#else
    #include <unistd.h>
    #include <sys/wait.h>
    #include <stdarg.h>

    #define _P_WAIT 0
    #define _P_NOWAIT 1

    #define MAX_SPAWN_ARGS 256

    int _spawnlp(int mode, const char *cmd, const char *arg0, ...) {
        char *args[MAX_SPAWN_ARGS];
        int argc = 0;

        args[argc++] = (char *)arg0;

        va_list valist;
        va_start(valist, arg0);

        char *next_arg = va_arg(valist, char *);
        while (next_arg != NULL && argc < (MAX_SPAWN_ARGS - 1)) {
            args[argc++] = next_arg;
            next_arg = va_arg(valist, char *);
        }
        args[argc] = NULL;
        va_end(valist);

        pid_t pid = fork();

        if (pid < 0) {
            return -1;
        }

        if (pid == 0) {
            execvp(cmd, args);
            exit(127);
        } else {
            if (mode == _P_WAIT) {
                int status;
                waitpid(pid, &status, 0);
                if (WIFEXITED(status)) {
                    return WEXITSTATUS(status);
                }
                return -1;
            }
            return pid;
        }
    }
#endif

static void gaussian_weights(int n, char *buf, size_t bufsz) {
    double sigma = n / 6.0;
    double center = (n - 1) / 2.0;
    double w[64];
    double sum = 0;
    for (int i = 0; i < n; i++) {
        double x = i - center;
        w[i] = exp(-(x * x) / (2 * sigma * sigma)); // ouch kai
        sum += w[i];
    }
    int pos = 0;
    for (int i = 0; i < n; i++) {
        int wi = (int)round(w[i] / sum * 1000);
        pos += snprintf(buf + pos, bufsz - pos, "%d%s", wi, i < n-1 ? "|" : "");
    }
}

void check_and_download_ffmpeg() {
#if defined(_WIN32) || defined(_WIN64)
    printf("ffmpeg not found. download now? (windows) (y/n): ");
    char choice = getch();
    if (choice == 'y' || choice == 'Y') {
        system("powershell -Command \"[Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12; "
       "New-Item -ItemType Directory -Force -Path 'ffmpeg_bin'; "
       "Invoke-WebRequest -Uri 'https://www.' 'gyan.dev' '/ffmpeg/builds/ffmpeg-release-essentials.zip' "
       "-OutFile 'ffmpeg_bin/ffmpeg.zip' -UseBasicParsing; "
       "Expand-Archive -Path 'ffmpeg_bin/ffmpeg.zip' -DestinationPath 'ffmpeg_bin'; "
       "Get-ChildItem -Path 'ffmpeg_bin/*-essentials_build/bin/*' | Move-Item -Destination 'ffmpeg_bin'; "
       "Remove-Item -Recurse -Force 'ffmpeg_bin/ffmpeg.zip', 'ffmpeg_bin/*-essentials_build'\"");
    }
#elif defined(__APPLE__) && defined(__MACH__)
    #include <TargetConditionals.h>
    #if TARGET_OS_OSX
        #if defined(__arm64__) || defined(__aarch64__)
            printf("you have an mac of silicon, sorry bye.");
        #elif defined(__x86_64__) || defined(_M_X64)
            printf("you have an mac of x86, not recommended");
            char choice = getch();
            if (choice == 'y' || choice == 'Y') {
                system("curl -L -s https://evermeet.cx -o ffmpeg.zip && unzip -q ffmpeg.zip && rm ffmpeg.zip");
            }
        #endif
    #else
        printf("no iphone sorry.");
    #endif
#else
    printf("ffmpeg not found. download now? (posix) (y/n): ");
    char choice = getch();
    if (choice == 'y' || choice == 'Y') {
        system("curl -L -s https://johnvansickle.com -o ffmpeg.tar.xz && tar -xf ffmpeg.tar.xz --strip-components=1 && rm ffmpeg.tar.xz");
    }
#endif
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("no input sorrey.\n");
        // system("pause"); // bad idea ohno
        printf("usage: %s input_file\n", argv[0]);
        printf("press any key to exit.\n"); // thats the good idea, good PainGest
        getch();
        return 1;
    }
    char *filename = argv[1];
    printf("frames (default 7): ");
    char input[16];
    fgets(input, sizeof(input), stdin);
    int frames = (strlen(input) > 1) ? atoi(input) : 7; // le defaulti 7
    if (frames < 1 || frames > 512) frames = 7;

    char output[1024];
    strncpy(output, filename, sizeof(output) - 1);
    char *dot = strrchr(output, '.');
    if (dot) *dot = '\0';
    strncat(output, "_blur.mp4", sizeof(output) - strlen(output) - 1); // end up with _blur.mp4 et the aend

    char filter[2048];
    printf("encoder (1=nvenc, 2=amf, 3=arc/qsv, 4=cpu): ");
    char enc_choice = getch();
    printf("%c\n", enc_choice);

    char *encoder;
    char *bitrate;
    if (enc_choice == '1') {
        encoder = "h264_nvenc";
        bitrate = "-rc vbr -cq 14";
    } else if (enc_choice == '2') {
        encoder = "h264_amf";
        bitrate = "-rc vbr_latency -qv 14";
    } else if (enc_choice == '3') {
        encoder = "h264_qsv";
        bitrate = "-global_quality 14";
    } else {
        encoder = "libx264";
        bitrate = "-crf 14";
    }
    printf("%s\n", bitrate);

    printf("options:\n1. equal\nother key. gaussian_sym\ninput: ");
    char choice = getch();
    char weights[1024];
    if (choice == '1') {
        strcpy(weights, "1");
    } else {
        gaussian_weights(frames, weights, sizeof(weights));
    }
    printf("%c\n", choice);
    char out_fps[8] = "60";
    printf("output fps: ");
    fgets(out_fps, sizeof(out_fps), stdin);
    out_fps[strcspn(out_fps, "\n")] = '\0';
    snprintf(filter, sizeof(filter), "tmix=frames=%d:weights=%s,fps=%s", frames, weights, out_fps);
    printf("filter: %s\n", filter);
    // snprintf(filter, sizeof(filter), "tmix=frames=%s:weights=gaussian_sym", frames);
    printf("Processing: %s -> %s (frames: %d)\n", filename, output, frames);
    // _spawnlp(_P_NOWAIT, "ffplay", "ffplay",
    //     "-window_title", "dildobean",
    //     "-probesize", "32",
    //     "-autoexit",
    //     "pipe:0",
    //     NULL);
    intptr_t status = _spawnlp(_P_WAIT, "ffmpeg", "ffmpeg",
                               "-i", filename,
                               "-vf", filter,
                               "-c:a", "copy",
                               "-c:v", encoder,
                               bitrate,
                               "-y",
                               output,
                               NULL);

    if (status == 0) {
        printf("success!\n");
    } else {
        printf("error: ffmpeg returned %lld.\n", (long long)status); // longlong penuas
    }
    return 0;
}