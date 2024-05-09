#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <ctype.h>

pthread_mutex_t varMutex = PTHREAD_MUTEX_INITIALIZER; // for variables
pthread_mutex_t fileMutex = PTHREAD_MUTEX_INITIALIZER; // for files
pthread_t threads[999]; // array for thread ids

// global variables
int totalFiles = 0; // number of total files
int errors = 0; // number of misspelled words
int totalErrorAll = 0; // total amount amongst all files
int threadCount = 0; // ** GET RID OF THIS NO LONGER NEED **

// passing multiple arguments to threads
typedef struct {
    char dictFile[99];
    char textFile[99];
} ThreadArguments;

// for misspelled words
typedef struct {
    char word[99];
    int count;
} MisspelledWord;

MisspelledWord *totalMisspelled; // total number amongst files

// helper function to check if the word is in the dictionary provided
// for spellChecker function
int inDict(char *word, char (*dict)[99], int dictSize) {
    for (int i = 0; i < dictSize; i++) {
        if (strcmp(word, dict[i]) == 0) {
            return 1; // word was found
        }
    }
    return 0; // word was not found
}

// helper function to lowercase the words passed in
// for spellChecker function
void toLowercase(char *str) {
    for (int i = 0; str[i] != '\0'; i++) {
        str[i] = tolower((unsigned char)str[i]);
    }
}

// helper function to simply open the dictionary file (american-english)
// for spellChecker function
void loadDict(char (*dict)[99], int *dictSize, const char* dictFileName) {
    FILE *dictFile = fopen(dictFileName, "r");
    if (!dictFile) {
        *dictSize = -1; // failed to open
        return;
    }

    // will read the words into the array
    while (!feof(dictFile)) {
        if (fgets(dict[*dictSize], 99, dictFile)) {
            toLowercase(dict[*dictSize]);
            size_t len = strlen(dict[*dictSize]);
            if (len > 0 && dict[*dictSize][len - 1] == '\n') {
                // removes the newline character
                dict[*dictSize][len - 1] = '\0';
            }
            (*dictSize)++; // increment sizing
        }
    }
    fclose(dictFile);
}

// helper function to sort the top 3 words, uses bubble sort
// for spellChecker function
void sortWords(MisspelledWord* misspelledWords, int numMisspelled) {
    // limits to top 3 or less
    int limit = (numMisspelled < 3) ? numMisspelled : 3;
    for (int i = 0; i < limit - 1; ++i) {
        for (int j = 0; j < limit - i - 1; ++j) {
            if (misspelledWords[j].count < misspelledWords[j + 1].count) {
                // simple bubble sort implementation
                MisspelledWord temp = misspelledWords[j];
                misspelledWords[j] = misspelledWords[j + 1];
                misspelledWords[j + 1] = temp;
            }
        }
    }
}

// helper function to write the results to a file
// for spellChecker function
void resultsToFile(const char* textFileName, int totalErrors, MisspelledWord *misspelledWords, int numMisspelled) {
    // LOCK
    pthread_mutex_lock(&fileMutex);
    FILE *resultFile = fopen("pvidena_A2.out", "a");
    if (!resultFile) {
        pthread_mutex_unlock(&fileMutex); // will unlock if it failed
        return;
    }

    // prints it to the file
    fprintf(resultFile, "%s: %d", textFileName, totalErrors);
    for (int i = 0; i < 3 && i < numMisspelled; i++) {
        fprintf(resultFile, " %s", misspelledWords[i].word);
    }
    fprintf(resultFile, "\n");
    fclose(resultFile);

    // UNLOCK
    pthread_mutex_unlock(&fileMutex);
}

// helper function to just update all the values
// for spellChecker function
void updateGlobalData(pthread_mutex_t *varMutex, int *totalFiles, int *totalErrorAll, int totalErrors, MisspelledWord *misspelledWords, int numMisspelled, MisspelledWord *totalMisspelled, int *errors) {
    // LOCK
    pthread_mutex_lock(varMutex);

    // updating the global counters
    (*totalFiles)++;
    (*totalErrorAll) += totalErrors;

    // updates the list of misspelled words
    for (int i = 0; i < numMisspelled; i++) {
        int found = 0;
        for (int j = 0; j < *errors; j++) {
            if (strcmp(totalMisspelled[j].word, misspelledWords[i].word) == 0) {
                totalMisspelled[j].count += misspelledWords[i].count;
                found = 1;
                break;
            }
        }
        if (!found) {
            strcpy(totalMisspelled[*errors].word, misspelledWords[i].word);
            totalMisspelled[*errors].count = misspelledWords[i].count;
            (*errors)++;
        }
    }

    // UNLOCK
    pthread_mutex_unlock(varMutex);
}

// helper function to cleanup in spellChecker function
// for spellChecker function
void cleanup(char (*dict)[99], MisspelledWord *misspelledWords, ThreadArguments *args) {
    // cleanup cleanup everybody cleanup
    free(dict); 
    free(misspelledWords);
    free(args);
}

// main function that does all of the heavy lifting, the "thread" you could say
void *spellChecker(void *arg) {
    ThreadArguments *args = (ThreadArguments *)arg;
    int dictSize = 0; // size of dict
    int amountOfMisspelled = 0; // number of words found misspelled
    int totalErrors = 0; // total found in the file
    char word[99]; // buffer

    // basic malloc and checking
    char(*dict)[99] = malloc(500000 * sizeof(*dict));
    MisspelledWord *misspelledWords = malloc(999 * sizeof(MisspelledWord));
    loadDict(dict, &dictSize, args->dictFile);
    FILE *textFile = fopen(args->textFile, "r");

    // simple error checking, frees if something messed up
    if (!dict || !misspelledWords || dictSize == -1 || !textFile) {
        cleanup(dict, misspelledWords, args);
        return NULL;
    }

    // will read and process each file
    while (fscanf(textFile, "%s", word) == 1) {
        size_t len = strlen(word);
        // helper to lowercase each word
        toLowercase(word);
        // checks if it is in the alphabet
        if (!isalpha(word[len - 1])) {
            word[len - 1] = '\0';
        }
        // checks if the word is in the dictionary
        if (!inDict(word, dict, dictSize)) {
            totalErrors++;
            int found = 0;
            // checks if it is already in the list
            for (int i = 0; i < amountOfMisspelled; i++) {
                if (strcmp(word, misspelledWords[i].word) == 0) {
                    misspelledWords[i].count++;
                    found = 1;
                    break;
                }
            }
            if (!found && amountOfMisspelled < 999) {
                // adds a new word to the misspelled list
                strcpy(misspelledWords[amountOfMisspelled].word, word);
                misspelledWords[amountOfMisspelled].count = 1;
                amountOfMisspelled++;
            }
        }
    }
    fclose(textFile);

    sortWords(misspelledWords, amountOfMisspelled); // calls the sorting helper function to determine top 3
    updateGlobalData(&varMutex, &totalFiles, &totalErrorAll, totalErrors, misspelledWords, amountOfMisspelled, totalMisspelled, &errors); // helper function to update the global data
    resultsToFile(args->textFile, totalErrors, misspelledWords, amountOfMisspelled); // gets the results and prints it to my pvidena file
    cleanup(dict, misspelledWords, args); // simple cleanup

    // je suis fini
    return NULL;
}







// helper function for sorting words again, like the one in spellChecker
// for main function
void sortWordsTwo(MisspelledWord *words, int wordCount) {
    // limits to top 3 again like previous bubble sort one
    int limit = (wordCount < 3) ? wordCount : 3;
    for (int i = 0; i < limit - 1; ++i) {
        for (int j = 0; j < limit - i - 1; ++j) {
            if (words[j].count < words[j + 1].count) {
                // simple bubble sort implementation
                MisspelledWord temp = words[j];
                words[j] = words[j + 1];
                words[j + 1] = temp;
            }
        }
    }
}

int main(int argc, char *argv[]) {
    // allocation for totalMisspelled
    totalMisspelled = malloc(999 * sizeof(MisspelledWord));
    totalErrorAll = 0;
    int myLoop = 1; // control var for main loop
    int lflag = 0; // for the lflag thing in commandline

    // checks for the -l flag, will set it accordingly
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-l") == 0) {
            lflag = 1;
            break;
        }
    }

    // loop for user input
    while (myLoop) {
        printf("Main Menu:\n");
        printf("1. Start a new spellchecking task\n");
        printf("2. Exit\n");
        printf("Enter your option: ");

        int userInput;
        scanf("%d", &userInput);

        if(userInput == 1){
            printf("\nSub Menu:\n");
            printf("Enter the dictionary file: ");
            char dictFile[99]; 
            char textFile[99];

            // clears any input
            while (getchar() != '\n');
            // get the dict name from user
            fgets(dictFile, sizeof(dictFile), stdin);
            size_t len = strlen(dictFile);
            if (len > 0 && dictFile[len - 1] == '\n') {
                // rid of any trailing new line
                dictFile[len - 1] = '\0';
            }

            // gets file name from user
            printf("Enter for the text file: ");
            fgets(textFile, sizeof(textFile), stdin);
            len = strlen(textFile);
            if (len > 0 && textFile[len - 1] == '\n') {
                textFile[len - 1] = '\0';
            }

            // THREAD TIMEEEEEEE
            ThreadArguments *args = (ThreadArguments *)malloc(sizeof(ThreadArguments));
            if (args == NULL) {
                exit(EXIT_FAILURE);
            }

            // copy the names into the threads
            strncpy(args->dictFile, dictFile, sizeof(args->dictFile) - 1);
            strncpy(args->textFile, textFile, sizeof(args->textFile) - 1);

            // creates the thread
            pthread_create(&threads[threadCount], NULL, spellChecker, args);
            pthread_detach(threads[threadCount]); // detach thread

            // increments thread count
            threadCount++;
            printf("\n");
        }
        else if(userInput == 2){
            printf("\n");
            myLoop = 0;
            break;
        }
        else{
            printf("Invalid Input\n");
        }
    }

    // call helper to sort before writing to file or terminal
    sortWordsTwo(totalMisspelled, errors);

    // checks for the lflag for writing results to file or terminal
    if (lflag == 0){
        printf("Number of files processed: %d\n", totalFiles);
        printf("Number of spelling errors: %d\n", totalErrorAll);
        printf("Three most common misspellings: ");
        // printing and formatting shenanigans
        for (int i = 0; i < 3; i++) {
            // prints first two items with commas
            if (i < 2) {
                printf("%s (%d times), ", totalMisspelled[i].word, totalMisspelled[i].count);
            }
            // prints last item with a period at the end of it
            else{
                printf("%s (%d times).\n", totalMisspelled[i].word, totalMisspelled[i].count);
            }
        }
    } 
    else{
        FILE *fptr = fopen("pvidena_A2.out", "a");
        if (fptr != NULL) {
            fprintf(fptr, "Number of files processed: %d\n", totalFiles);
            fprintf(fptr, "Number of spelling errors: %d\n", totalErrorAll);
            fprintf(fptr, "Three most common misspellings:\n");
            // printing and formatting shenanigans
            for (int i = 0; i < 3; i++) {
                // prints first two items with commas
                if (i < 2) {
                    fprintf(fptr, "%s (%d times), ", totalMisspelled[i].word, totalMisspelled[i].count);
                }
                // prints last item with a period at the end of it
                else{
                    fprintf(fptr, "%s (%d times).\n", totalMisspelled[i].word, totalMisspelled[i].count);
                }
            }
            fclose(fptr);
        }
    }

    // freeeeeeeeeeeeeeeeee
    free(totalMisspelled);

    // je suis fini
    return 0;
}
