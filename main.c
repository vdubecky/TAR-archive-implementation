#include <stdlib.h>
#include "archive.h"

bool parameter_check(const short index, const char char_param, param *parameters)
{
    if (parameters->stats[index]) {
        fprintf(stderr, "Duplicit option '%c' found in program arguments\n", char_param);
        return false;
    }
    parameters->stats[index] = true;
    if (parameters->stats[1] && parameters->stats[2]) {
        fprintf(stderr, "Unknown or conflicting option '%c' found in command arguments\n", char_param);
        return false;
    }
    return true;
}


void free_param(param *parameters)
{
    for (int i = 0; i < parameters->file_count; i++) {
        free(parameters->sources[i]);
    }

    free(parameters->sources);
}


int main(int argc, char **argv)
{
    if (argc < 3) {
        fprintf(stderr, "Bad argument count \n");
        return EXIT_FAILURE;
    }
    param parameters = { .sources = NULL, .file_count = 0, .archive_name = NULL, .stats[0] = false, .stats[1] = false, .stats[2] = false };
    size_t size = strlen(argv[1]);
    for (size_t i = 0; i < size; i++) {
        switch (argv[1][i]) {
        case 'v':
            if (!parameter_check(0, 'v', &parameters))
                return EXIT_FAILURE;
            break;
        case 'c':
            if (!parameter_check(1, 'c', &parameters))
                return EXIT_FAILURE;
            break;
        case 'x':
            if (!parameter_check(2, 'x', &parameters))
                return EXIT_FAILURE;
            break;
        default:
            fprintf(stderr, "Unknown or conflicting option '%c' found in command arguments\n", argv[1][i]);
            return EXIT_FAILURE;
        }
    }
    if (!parameters.stats[1] && !parameters.stats[2]) {
        fprintf(stderr, "No operation mode given - one of c/x is required\n");
        return EXIT_FAILURE;
    }

    parameters.archive_name = argv[2];
    parameters.sources = malloc((argc - 3) * sizeof(char *));
    parameters.file_count = argc - 3;

    for (int i = 3; i < argc; i++) {
        parameters.sources[i - 3] = malloc(strlen(argv[i]) + 1);
        strcpy(parameters.sources[i - 3], argv[i]);
    }

    if (!execute(&parameters)) {
        free_param(&parameters);
        return EXIT_FAILURE;
    }

    free_param(&parameters);
    return EXIT_SUCCESS;
}
