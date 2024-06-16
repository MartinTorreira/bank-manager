#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/time.h>
#include "options.h"

#define MAX_AMOUNT 20
int dep, trans = 0;

struct bank {
    int num_accounts;        // number of accounts
    int *accounts;           // balance array
    bool          isFinish;     // boolean (transfers)
    pthread_mutex_t *mutexes;   //mutex array

};

struct args {
    int          thread_num;  // application defined thread #
    int          delay;       // delay between operations
    int	         iterations;  // number of operations
    int          net_total;   // total amount deposited by this thread
    struct bank *bank;        // pointer to the bank (shared with other threads)
};

struct thread_info {
    pthread_t    id;    // id returned by pthread_create()
    struct args *args;  // pointer to the arguments
};

// Threads run on this function
void *deposit(void *ptr) {
    struct args *args =  ptr;
    int amount, account, balance;

    while(args->iterations--) {
        amount  = rand() % MAX_AMOUNT;
        account = rand() % args->bank->num_accounts;

        printf("Thread %d depositing %d on account %d\n",
               args->thread_num, amount, account);


        //OP CRITICA INICIO
        pthread_mutex_lock(&args->bank->mutexes[account]);
        balance = args->bank->accounts[account];
        if(args->delay) usleep(args->delay); // Force a context switch

        balance += amount;
        if(args->delay) usleep(args->delay);

        args->bank->accounts[account] = balance;
        if(args->delay) usleep(args->delay);

        args->net_total += amount;
        pthread_mutex_unlock(&args->bank->mutexes[account]);
        //OP CRITICA FINAL
        //printf("-> Deposito nº: %d \n", ++dep); //Print que muestra numero de iteraciones
    }

    return NULL;
}

void *transfer(void *ptr) {

    struct args *args =  ptr;
    int amount, acc1, acc2, balanceOrg, balanceDest;

    while(args->iterations--){

        acc1 = rand() % args->bank->num_accounts;
        do acc2 = rand() % args->bank->num_accounts; while(acc1 == acc2);


        //OP CRITICA INICIO
        if(acc2 > acc1){
            pthread_mutex_lock(&args->bank->mutexes[acc1]);
            pthread_mutex_lock(&args->bank->mutexes[acc2]);
        }else{
            pthread_mutex_lock(&args->bank->mutexes[acc2]);
            pthread_mutex_lock(&args->bank->mutexes[acc1]);
        }

        amount = args->bank->accounts[acc1] != 0?         // amount to be
                 rand() % args->bank->accounts[acc1] : 0; // transferred

        balanceOrg = args->bank->accounts[acc1];
        balanceDest = args->bank->accounts[acc2];
        if(args->delay) usleep(args->delay); // Force a context switch

        balanceOrg -= amount;
        balanceDest += amount;
        if(args->delay) usleep(args->delay);

        args->bank->accounts[acc1] = balanceOrg;
        args->bank->accounts[acc2] = balanceDest;
        if(args->delay) usleep(args->delay);

        args->net_total += amount;

        //OP CRITICA FINAL
        pthread_mutex_unlock(&args->bank->mutexes[acc1]);
        pthread_mutex_unlock(&args->bank->mutexes[acc2]);

        args->net_total += amount;
        printf("Thread %d transferring %d on account %d from account %d\n",
               args->thread_num, amount, acc2, acc1);
        //printf("-> Transaccion nº: %d \n", ++trans); //printe que muestra numero de iteraciones
    }

    return NULL;
}

void *printAmount(void *ptr){

    struct args *args =  ptr;
    int amount, i;

    while(true){
        amount = 0;

        for (i = 0; i < args->bank->num_accounts; i++) pthread_mutex_lock(&args->bank->mutexes[i]);

        for (i = 0; i < args->bank->num_accounts; i++) amount += args->bank->accounts[i];

        for (i = 0; i < args->bank->num_accounts; i++) pthread_mutex_unlock(&args->bank->mutexes[i]);

        printf(" ##############  Total amount: %d  ############## \n", amount);
        if(args->bank->isFinish) break;
        else usleep(args->delay);
    }

    return NULL;
}


// start opt.num_threads threads running on deposit.
struct thread_info *start_threads(struct options opt, struct bank *bank, void *(*func)(void*arg)) {
    int i,resto;
    struct thread_info *threads;

    if(opt.num_threads > opt.iterations){
        printf("Not enought iterations\n");
        exit(1);
    }

    printf("creating %d threads\n", opt.num_threads);
    threads = malloc(sizeof(struct thread_info) * opt.num_threads);

    if (threads == NULL) {
        printf("Not enough memory\n");
        exit(1);
    }

    resto = opt.iterations%opt.num_threads;

    // Create num_thread threads running swap()
    for (i = 0; i < opt.num_threads; i++) {

        threads[i].args = malloc(sizeof(struct args));
        threads[i].args -> thread_num = i;
        threads[i].args -> net_total  = 0;
        threads[i].args -> bank       = bank;
        threads[i].args -> delay      = opt.delay;

        if(i==0)
            threads[i].args-> iterations = (opt.iterations / opt.num_threads) + resto;
        else
            threads[i].args-> iterations = (opt.iterations / opt.num_threads);


        if (0 != pthread_create(&threads[i].id, NULL, func, threads[i].args)) {
            printf("Could not create thread #%d", i);
            exit(1);
        }
    }
    return threads;
}


// Print the final balances of accounts and threads
void print_balancesThread(struct thread_info *thrs, int num_threads) {

    if(thrs->args->bank->isFinish) return;

    int total_deposits=0;
    printf("\nNet amounts moved by thread\n");

    for(int i=0; i < num_threads; i++) {
        printf("%d: %d\n", i, thrs[i].args->net_total);
        total_deposits += thrs[i].args->net_total;
    }
    printf("Total: %d\n", total_deposits);
}

void print_BalancesAccount(struct bank *bank, int num_threads) {
    int bank_total=0;
    printf("\nAccount balance\n");
    for(int i=0; i < bank->num_accounts; i++) {
        printf("%d: %d\n", i, bank->accounts[i]);
        bank_total += bank->accounts[i];
    }
    printf("Total: %d\n", bank_total);
}


// wait for all threads to finish, print totals, and free memory
void wait(struct options opt, struct bank *bank, struct thread_info *threads) {
    // Wait for the threads to finish
    for (int i = 0; i < opt.num_threads; i++)
        pthread_join(threads[i].id, NULL);

    //print_balances(bank, threads, opt.num_threads);
    print_balancesThread(threads, opt.num_threads);

    for (int i = 0; i < opt.num_threads; i++)
        free(threads[i].args);

    free(threads);
}

void freeBank(struct options opt, struct bank *bank){
    print_BalancesAccount(bank, opt.num_threads);

    free(bank->accounts);
    for (int i = 0; i < opt.num_accounts; i++) {
        pthread_mutex_destroy(&bank->mutexes[i]);
    }
    free(bank->mutexes);
}

// allocate memory, and set all accounts to 0
void init_accounts(struct bank *bank, int num_accounts) {
    bank->num_accounts = num_accounts;
    bank->accounts     = malloc(bank->num_accounts * sizeof(int));
    bank->mutexes      = malloc(bank->num_accounts * sizeof(pthread_mutex_t));
    bank->isFinish     = false;

    for(int i=0; i < bank->num_accounts; i++){
        bank->accounts[i] = 0;
        pthread_mutex_init(&bank->mutexes[i], NULL);
    }
}

int main (int argc, char **argv)
{
    struct options      opt, optP;
    struct bank         bank;
    struct thread_info *thrsD, *thrsT, *thrsP;

    srand(time(NULL));

    // Default values for the options
    opt.num_threads  = 5;
    opt.num_accounts = 10;
    opt.iterations   = 100;
    opt.delay        = 10;

    optP = opt;
    optP.num_threads = 1;

    read_options(argc, argv, &opt);

    init_accounts(&bank, opt.num_accounts);
    
    //Threads deposit
    thrsD = start_threads(opt, &bank, deposit);
    wait(opt, &bank, thrsD);

    //Threads transfer
    thrsT = start_threads(opt, &bank,  transfer);

    //Threads total bank amount
    thrsP = start_threads(optP, &bank, printAmount);
    wait(opt, &bank, thrsT);
    bank.isFinish = true;
    wait(optP, &bank, thrsP);

    freeBank(opt, &bank);

    return 0;
}