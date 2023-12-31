//采用neon指令集优化，并且进行了对齐操作，并且采用了静态线程 +barrier 同步多线程方式
#include <iostream>
#include<pthread.h>
#include<stdlib.h>
#include<stdio.h>
#include<sys/time.h>
#include <fstream>
#include <sstream>
#include<bitset>
#include<arm_neon.h>
#define NUM_THREADS 7
using namespace std;
const int Columnnum = 1011;
const int Rnum = 539;
const int Enum = 263;
const int ArrayColumn = 32;
const int leftbit =13;
unsigned int R [Columnnum][ArrayColumn];
unsigned int E [Enum][ArrayColumn];
int First[Enum];
bitset<32> MyBit(0);
char fin[100000] = {0};
bool IsNULL[Columnnum] = {1};
pthread_barrier_t xor_barrier;
pthread_barrier_t setr_barrier;
bool flag = 1;
//线程数据结构定义
typedef struct{
    int t_id;//线程id
}threadParam_t;
//barrier定义
pthread_barrier_t mybarrier;
int Find_First(int index){
    int j = 0;
    int cnt = 0;
    while(E[index][j] == 0){
        j++;
        if(j == ArrayColumn) break;
    }
    if(j == ArrayColumn) return -1;
    unsigned int tmp = E[index][j];
    while(tmp != 0){
        tmp = tmp >> 1;
        cnt++;
    }
    return Columnnum - 1 - ((j+1)*32 - cnt - leftbit);
}
void Init_R(){
    unsigned int a;
    ifstream infile("/home/data/Groebner/4_1011_539_263/1.txt");

    int index;
    while(infile.getline(fin,sizeof(fin))){
        std::stringstream line(fin);
        bool flag = 0;
        while(line >> a){
            if(flag == 0){
                index = a;
                flag = 1;
            }
            int k = a % 32;
            int j = a / 32;
            int temp = 1 << k;
            R[index][ArrayColumn - 1 - j] += temp;
        }
    }
}
void Init_E(){
    unsigned int a;
    ifstream infile("/home/data/Groebner/4_1011_539_263/2.txt");

    int index = 0;
    while(infile.getline(fin,sizeof(fin))){
        std::stringstream line(fin);
        int flag = 0;
        while(line >> a){
            if(flag == 0){
                First[index] = a;
                flag = 1;
            }
            int k = a % 32;
            int j = a / 32;
            int temp = 1 << k;
            E[index][ArrayColumn - 1 - j] += temp;
        }
        index++;
    }
}
void Init_IsNULL(){
    for(int i = 0;i < Columnnum; i++){
        bool flag = 0;
        for(int j = 0;j < ArrayColumn; j++){
            if(R[i][j] != 0){
                flag = 1;
                IsNULL[i] = 0;
                break;
            }
        }
        if(flag == 0) IsNULL[i] = 1;
    }
}
void Set_R(int eindex,int rindex){
    for(int j = 0;j < ArrayColumn; j++){
        R[rindex][j] = E[eindex][j];
    }
}
void Align_XOR(int eindex,int rindex){//we do parallel programming here.
    uint32x4_t vaR;
    uint32x4_t vaE;
    int j = 0;
    while((eindex * ArrayColumn + j) % 4 != 0){//we do alignment here.
        E[eindex][j] = E[eindex][j] ^ R[rindex][j];
        j++;
    }
    for(;j + 4 <= ArrayColumn; j += 4){
        vaE = vld1q_u32(&(E[eindex][j]));
        vaR = vld1q_u32(&(R[rindex][j]));
        vaE = veorq_u32(vaE,vaR);
        vst1q_u32(&(E[eindex][j]),vaE);
    }
    for(;j < ArrayColumn; j++){
        E[eindex][j] = E[eindex][j] ^ R[rindex][j];
    }

}
//线程函数定义
void *threadFunc(void *param){
    threadParam_t *p = (threadParam_t*)param;
    int t_id = p->t_id;
    while(1){
        for(int i = t_id;i < Enum; i += NUM_THREADS){
            while(First[i] != -1){
                if(IsNULL[First[i]] == 0){
                    Align_XOR(i,First[i]);
                    First[i] = Find_First(i);
                }
                else{
                   // Set_R(i,First[i]);
                   // IsNULL[First[i]]=0;
                    break;
                }
            }
        }
        pthread_barrier_wait(&xor_barrier);


        if(t_id == 0)
        {
            for(int i = 0;i < Enum; i++){
                if(First[i] != -1){
                    if(IsNULL[First[i]] == 1){
                        Set_R(i,First[i]);
                        IsNULL[First[i]] = 0;
                        First[i] = -1;
                        break;
                    }
                }
            }
            flag = 1;
            for(int i = 0;i < Enum; i++){
                if(First[i] != -1) flag = 0;
            }
        }
        pthread_barrier_wait(&setr_barrier);
        if(flag == 1) {
            break;
        }
    }
    pthread_exit(NULL);
}
void Print(){//Print the answer
    for(int i = 0;i < Enum; i++){
        cout<<"*";
        bool isnull = 1;
        for(int j = 0;j < ArrayColumn; j++){
            if(E[i][j] != 0){
                isnull = 0;
                break;
            }
        }
        if(isnull){
            cout<<endl;
            continue;
        }
        for(int j = 0;j < ArrayColumn; j++){
            if(E[i][j] == 0) continue;
            MyBit = E[i][j];//MENTION: bitset manipulates from the reverse direction
            for(int k = 31;k >= 0; k--){
                if(MyBit.test(k)){
                    cout<<32 * (ArrayColumn - j - 1) + k<<' ';
                }
            }
        }
        cout<<endl;
    }
}
int main()
{

    struct timeval head;
    struct timeval tail;
    Init_R();
    Init_E();
    Init_IsNULL();
    //初始化barrier
    pthread_barrier_init(&xor_barrier,NULL,NUM_THREADS);
    pthread_barrier_init(&setr_barrier,NULL,NUM_THREADS);
    //创建线程
    pthread_t handles[NUM_THREADS];//创建对应的handle
    threadParam_t param[NUM_THREADS];//创建对应的线程数据结构
    gettimeofday(&head,NULL);
    for(int t_id = 0;t_id < NUM_THREADS;++t_id){
        param[t_id].t_id = t_id;
        pthread_create(&handles[t_id],NULL,threadFunc,(void*)&param[t_id]);
    }

    for(int t_id = 0;t_id < NUM_THREADS;++t_id){
        pthread_join(handles[t_id],NULL);
    }
    gettimeofday(&tail,NULL);
    cout<<"N: "<<Columnnum<<" Time: "<<(tail.tv_sec-head.tv_sec)*1000.0+(tail.tv_usec-head.tv_usec)/1000.0<<"ms"<<endl;
    //销毁所有的barrier
    pthread_barrier_destroy(&mybarrier);
   // Print();
    return 0;
}
