#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <time.h>
#include <stdlib.h>
#include <fcntl.h>
#include <dirent.h>
#include <unistd.h>
#include <cstring>
#include <iostream>
// #include <cstdlib>
// #include <ctime>
// #include <cstdint>
#define REG(address) *(volatile unsigned int*)(address)
#define N_MAX 100


int reset_pl_resetn0(){
	int fd;
	char attr[32];

	DIR *dir = opendir("/sys/class/gpio/gpio510");
	if (!dir) {
		fd = open("/sys/class/gpio/export", O_WRONLY);
		if (fd < 0) {
			perror("open(/sys/class/gpio/export)");
			return -1;
		}
		strcpy(attr, "510");
		write(fd, attr, strlen(attr));
		close(fd);
		dir = opendir("/sys/class/gpio/gpio510");
		if (!dir) {
			return -1;
		}
	}
	closedir(dir);

	fd = open("/sys/class/gpio/gpio510/direction", O_WRONLY);
	if (fd < 0) {
		perror("open(/sys/class/gpio/gpio510/direction)");
		return -1;
	}
	strcpy(attr, "out");
	write(fd, attr, strlen(attr));
	close(fd);

	fd = open("/sys/class/gpio/gpio510/value", O_WRONLY);
	if (fd < 0) {
		perror("open(/sys/class/gpio/gpio510/value)");
		return -1;
	}
	sprintf(attr, "%d", 0);
	write(fd, attr, strlen(attr));

    sprintf(attr, "%d", 1);
	write(fd, attr, strlen(attr));
	close(fd);

	return 0;
}

volatile int* DMEM_BASE;
// unsigned int* DMEM_BASE;
volatile float* DMEM_BASE_FLOAT;
volatile unsigned int* IMEM_BASE;
volatile unsigned int* GPIO_DATA;
volatile unsigned int* GPIO_TRI;
#define DMEM_OFFSET 1024*16

void load_data(std::string filename, int* n, float cost[][N_MAX], int* x, int* y){
    // std::cout << "Load Start" << std::endl;
    FILE* fp = fopen(filename.c_str(), "rb");
    if(fp == NULL) {
        std::cout << "Load error" << std::endl;
        return;
    }
    fread(n, sizeof(int), 1, fp);
    for(int i=0; i<*n; i++){
        for(int j=0; j<*n; j++){
            double tmp;
            fread(&tmp, sizeof(double), 1, fp);
            cost[i][j] = (float)tmp;
        }
    }
    fread(x, sizeof(int), *n, fp);
    fread(y, sizeof(int), *n, fp);
    fclose(fp);
    // std::cout << "Load End" << std::endl;

}

bool verify(int n, int* dump_x, int* dump_y, volatile int* riscv_x, volatile int* riscv_y){
    // std::cout << "Verify Start" << std::endl;
    bool ok = true;
    for(int i = 0; i < n; i++){
        // std::cout << dump_x[i] << " " << riscv_x[i] << std::endl;
        if(dump_x[i] != riscv_x[i]){
            std::cout << "i=" << i << " dump_x[i] != riscv_x[i]" << dump_x[i] << " " << riscv_x[i] << std::endl;
            ok = false;
        }
        if (dump_y[i] != riscv_y[i]){
            std::cout << "i=" << i << " dump_y[i] != riscv_y[i]" << dump_y[i] << " " << riscv_y[i] << std::endl;
            ok = false;
        }
    }
    // std::cout << "Verify End" << std::endl;
    return ok;
}


void set_input(int n, float cost[N_MAX][N_MAX]){
    // std::cout << "Set Input Start" << std::endl;
    DMEM_BASE[DMEM_OFFSET+0] = n;
    volatile float* DMEM_BASE_FLOAT = (volatile float*) DMEM_BASE;
    for(int i = 0; i < n; i++){
        for(int j = 0; j < n; j++){
            DMEM_BASE_FLOAT[DMEM_OFFSET+i*n+j+1] = cost[i][j];
        }
    }
    // std::cout << "Set Input End" << std::endl;
}

#include <chrono>
bool process_data(int i){
    int dump_n;
    float dump_cost[N_MAX][N_MAX];
    int dump_x[N_MAX];
    int dump_y[N_MAX];
    std::string filename = "./data/data_" + std::to_string(i);
    load_data(filename.c_str(), &dump_n, dump_cost, dump_x, dump_y);
    std::chrono::system_clock::time_point  t1, t2, t3;
    t1 = std::chrono::system_clock::now();
    set_input(dump_n, dump_cost);
    //set incomplete flag
    DMEM_BASE[DMEM_OFFSET+(1+dump_n*dump_n+dump_n*2)] = 0;
    std::cout << "n: " << dump_n << std::endl;
    reset_pl_resetn0();
    t2 = std::chrono::system_clock::now();
    while(1){
        bool endflag = DMEM_BASE[DMEM_OFFSET+(1+dump_n*dump_n+dump_n*2)] == dump_n*2;
        if(endflag) break;
        usleep(1);
    }
    t3 = std::chrono::system_clock::now();
    double elapsed1 = (double)std::chrono::duration_cast<std::chrono::microseconds>(t2-t1).count();
    double elapsed2 = (double)std::chrono::duration_cast<std::chrono::microseconds>(t3-t2).count();
    std::cout << "set input:" << elapsed1 << "[microsec]" << std::endl;
    std::cout << "      run:" << elapsed2 << "[microsec]" << std::endl;

    volatile int* riscv_x = &DMEM_BASE[DMEM_OFFSET+1+dump_n*dump_n];
    volatile int* riscv_y = &DMEM_BASE[DMEM_OFFSET+1+dump_n*dump_n+dump_n];
    return verify(dump_n, dump_x, dump_y, riscv_x, riscv_y);
}

// bool process_data(int i){
//     float* DMEM_BASE_FLOAT = (float*) DMEM_BASE;
//     std::cout << "hoge" << std::endl;
//     DMEM_BASE_FLOAT[DMEM_OFFSET+0] = 0.4f;
//     DMEM_BASE_FLOAT[DMEM_OFFSET+1] = 0.3f;
//     reset_pl_resetn0();
//     sleep(1);
//     for(int i = 0; i < 10; i++){
//         std::cout << DMEM_BASE_FLOAT[DMEM_OFFSET+i] << std::endl;
//     }
// }

extern unsigned int riscv_imm(volatile unsigned int *IMEM );
int main()
{
    int uio0_fd = open("/dev/uio0", O_RDWR | O_SYNC);
    DMEM_BASE = (int*) mmap(NULL, 0x20000, PROT_READ|PROT_WRITE, MAP_SHARED, uio0_fd, 0);
    // DMEM_BASE_FLOAT = (float*) mmap(NULL, 0x40000, PROT_READ|PROT_WRITE  , MAP_SHARED, uio0_fd, 0);
    // DMEM_BASE_FLOAT = (float*) mmap(NULL, 0x2000, PROT_READ|PROT_WRITE, MAP_SHARED, uio0_fd, 0);
    int uio1_fd = open("/dev/uio1", O_RDWR | O_SYNC);
    IMEM_BASE = (unsigned int*) mmap(NULL, 0x10000, PROT_READ|PROT_WRITE, MAP_SHARED, uio1_fd, 0);
    int uio2_fd = open("/dev/uio2", O_RDWR | O_SYNC);
    GPIO_DATA = (unsigned int*) mmap(NULL, 0x10000, PROT_READ|PROT_WRITE, MAP_SHARED, uio2_fd, 0);
    GPIO_TRI = GPIO_DATA + 1;
    REG(GPIO_TRI) = 0x00;
    REG(GPIO_DATA) = 0x02; // LED0

    //write instruction
    riscv_imm(IMEM_BASE);

    // srand((unsigned)time(NULL));
    bool all_pass = true;
    for(int i = 0; i < 607; i++){
        if(!process_data(i)){
            std::cout << "Attempt " << i << " Fail" << std::endl;
            all_pass = false;
        } else {
            std::cout << "Attempt " << i << " Success" << std::endl;
        }
    }
    if (all_pass) std::cout << "ALL PASS" << std::endl;
    else std::cout << "FAILED" << std::endl;


    return 0;
}
