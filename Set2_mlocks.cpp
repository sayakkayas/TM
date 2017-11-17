//g++ Set2_mlocks.cpp  -std=c++0x -pthread -mrtm -o Set2_mlocks -w
//Run  - ./Set2_mlocks -t 4 -i 10000
#include <thread>
#include <string.h>
#include <stdlib.h>
#include <iostream>
#include <atomic>
#include <mutex>
#include <ctime>
#include <chrono>
#include <algorithm>
#include <utility>
#include <unistd.h>
#include <sys/syscall.h>
#include <immintrin.h>

#define gettid() syscall(SYS_gettid)
using namespace std;

/*Global variables for locks and time calculation*/
std::atomic<bool> start;


//for time keeping
std::chrono::high_resolution_clock::time_point t1,t2;
std::chrono::duration<double> time_span;
/*....*/


struct node
{
	int data;
	struct node* link;
};

struct node* head[12];
std::mutex cpp_lock[12];

/*---------------------------------------------------------------*/
//functions
void display(struct node* head)
{
	struct node* ptr;
	ptr=head;
	// printf("%d->",ptr->data);


	while(ptr!=NULL)
	{
		printf("%d->",ptr->data);
		ptr=ptr->link;
	}

	printf("\n");

}


int deletes(struct node** head)
{
	struct node* ptr;
	int e;
	ptr=(*head);

	if(ptr==NULL)
	{
		printf("List is empty\n");
		return -1;
	} 
	else
	{
		e=ptr->data;
		(*head)=ptr->link;
	}

	return e; 
}

void insert(struct node** head,int ele)
{
	struct node* tmp;
	tmp=(struct node*)malloc(sizeof(struct node));
	tmp->data=ele;
	tmp->link=NULL;

	if((*head)==NULL)
	{
		(*head)=tmp;
		//display(head0);
		//printf("here\n");
	} 
	else
	{
		if(ele <= (*head)->data)
		{
			tmp->link=(*head);
			(*head)=tmp;
		}
		else
		{ 	 
			struct node* ptr,*prev;
			prev=(*head);
			//printf("Here %d\n",ele);
			//printf("prev->data %d\n",prev->data);

			ptr=(*head)->link;
			//printf("Ptr->data %d\n",ptr->data);

			while(ptr!=NULL)
			{
				if(ptr->data > ele)
					break;
				else
				{
					prev=ptr;
					ptr=ptr->link;
				}             
			}

			tmp->link=ptr;
			prev->link=tmp;
		}

	}

}


int check(struct node* head)
{
	struct node* ptr;
	ptr=head;

	while(ptr!=NULL)
	{
		if(ptr->link!=NULL)
		{
			if(ptr->link->data < ptr->data)
				return 1;
		}

		ptr=ptr->link;
	}

	return 0;
}

void initialize(int iterations)
{
	int e;
	int i=0, j=0;

	for(j=0; j<12; j++)
	{	
		i=0;
		while(i<iterations)
		{ 
			e=rand()%100;
			insert(&head[j], e);
			i++;
		}
	}
	//display(head[0]);
}

void clearLists()
{
	int i;
	for(i=0;i<12;i++)
	{
		head[i]=NULL;

	}
}

void func_STM(int index1,int index2,int iter)
{

	while (!start.load());//spin until start becomes true.This is done so that all threads start at the same time (truely run parallely)

	pid_t tid = gettid();

	int i;
	int ele;
	for(i=0;i<iter;i++)
	{
		int status;
		synchronized
		{
			ele=deletes(&head[index1]);

			if(ele!=-1)
				insert(&head[index2],ele);
		} 
	}//for close             	
}


void func_TM(int index1,int index2,int iter)
{

	while (!start.load());//spin until start becomes true.This is done so that all threads start at the same time (truely run parallely)

	pid_t tid = gettid();

	int i;
	int ele;
	for(i=0;i<iter;i++)
	{
		int status;
		if ((status = _xbegin()) == _XBEGIN_STARTED) 
		{
			ele=deletes(&head[index1]);

			if(ele!=-1)
				insert(&head[index2],ele);

			// transaction  
			_xend();                                         // For gcc 4.8 use immintrin.h and -mrtm 
		} 
		else 
		{ 
			if(index1 < index2)
			{
				cpp_lock[index1].lock();   //acquire lock on the mutex _lock
				cpp_lock[index2].lock();   //acquire lock on the mutex _lock

				ele=deletes(&head[index1]);

				if(ele!=-1)
					insert(&head[index2],ele);

				cpp_lock[index2].unlock(); //release lock on the mutex _lock
				cpp_lock[index1].unlock(); //release lock on the mutex _lock
			}
			else
			{
				cpp_lock[index2].lock();   //acquire lock on the mutex _lock
				cpp_lock[index1].lock();   //acquire lock on the mutex _lock

				ele=deletes(&head[index1]);

				if(ele!=-1)
					insert(&head[index2],ele);

				cpp_lock[index1].unlock(); //release lock on the mutex _lock
				cpp_lock[index2].unlock(); //release lock on the mutex _lock
			}
		}//else close
	}//for close             	
}


void func_lock(int index1, int index2, int iter)
{
	while (!start.load());//spin until start becomes true.This is done so that all threads start at the same time (truely run parallely)

	pid_t tid = gettid();

	int i;
	int ele;

	for(i=0; i<iter; i++)
	{
		if(index1 < index2)
		{
			cpp_lock[index1].lock();   //acquire lock on the mutex _lock
			cpp_lock[index2].lock();   //acquire lock on the mutex _lock

			ele=deletes(&head[index1]);

			if(ele!=-1)
				insert(&head[index2],ele);

			cpp_lock[index2].unlock(); //release lock on the mutex _lock
			cpp_lock[index1].unlock(); //release lock on the mutex _lock
		}
		else
		{
			cpp_lock[index2].lock();   //acquire lock on the mutex _lock
			cpp_lock[index1].lock();   //acquire lock on the mutex _lock

			ele=deletes(&head[index1]);

			if(ele!=-1)
				insert(&head[index2],ele);

			cpp_lock[index1].unlock(); //release lock on the mutex _lock
			cpp_lock[index2].unlock(); //release lock on the mutex _lock
		}
	}
}

//Supervisor function that makes the threads and allotes them different task for each experiment and relaunches them with initial conditions for different experiments
void make_threads_joinsAndReLaunch(int num_threads,int iterations)
{
	int i;
	int index1[12];
	int index2[12];

	for(i=0;i<12;i++)
	{
		index1[i]=rand()%12;
		index2[i]=(index1[i]+1 )%12;
	}

	initialize(iterations*num_threads);

	std::thread myThreads[num_threads];      //initializing  threads

	//lock and unlock mechanism
	start=false; //initialize start to false so that each thread waits for every other thread to be created before it can start

	// int i;
	for (i=0; i<num_threads; i++)
		myThreads[i]=std::thread(func_lock, index1[i], index2[i], iterations);  //run N number of threads

	start=true;  //all threads start now

	t1=std::chrono::high_resolution_clock::now();   //get current time with fine granularity(start time for threads)
	for (i=0; i<num_threads; i++)
		myThreads[i].join();                               //wait for all the threads created to complete
	t2=std::chrono::high_resolution_clock::now();    //get current time when the threads have terminated

	time_span=std::chrono::duration_cast<std::chrono::duration<double>>(t2-t1); //calculate the difference of the two times to get the time required to execute
	cout <<"CPP Lock  Time " << time_span.count()<<" secs" << endl;

	clearLists();

	start=false;
	initialize(iterations*num_threads);

	for ( i=0; i<num_threads; i++)
		myThreads[i]=std::thread(func_TM,index1[i],index2[i],iterations);  //run N number of threads

	start=true;  //all threads start now

	t1=std::chrono::high_resolution_clock::now();   //get current time with fine granularity(start time for threads)
	for ( i=0; i<num_threads; i++)
		myThreads[i].join();                               //wait for all the threads created to complete
	t2=std::chrono::high_resolution_clock::now();    //get current time when the threads have terminated

	time_span=std::chrono::duration_cast<std::chrono::duration<double>>(t2-t1); //calculate the difference of the two times to get the time required to execute
	cout <<"RTM TM Time " << time_span.count()<<" secs" << endl;

	clearLists();

	start=false;
	initialize(iterations*num_threads);

	for ( i=0; i<num_threads; i++)
		myThreads[i]=std::thread(func_STM,index1[i],index2[i],iterations);  //run N number of threads

	start=true;  //all threads start now

	t1=std::chrono::high_resolution_clock::now();   //get current time with fine granularity(start time for threads)
	for ( i=0; i<num_threads; i++)
		myThreads[i].join();                               //wait for all the threads created to complete
	t2=std::chrono::high_resolution_clock::now();    //get current time when the threads have terminated

	time_span=std::chrono::duration_cast<std::chrono::duration<double>>(t2-t1); //calculate the difference of the two times to get the time required to execute
	//for (i=0; i<num_threads; i++){
	//	display(head[index1[i]]);
	//}
	cout <<"STM TM Time " << time_span.count()<<" secs" << endl;
}

/*------------------------------main method--------------------------------*/
int main(int argc,char** argv)
{
	int num_threads;
	int iterations;

	num_threads=4;
	iterations=10000;

	if(argc!=0)
	{
		if(argc==5)
		{   
			if(strcmp(argv[1],"-t")==0)
			{
				num_threads=atoi(argv[2]);
			}

			if(strcmp(argv[3],"-t")==0)
			{
				num_threads=atoi(argv[4]);
			}

			if(strcmp(argv[1],"-i")==0)
			{
				iterations=atoi(argv[2]);
			}

			if(strcmp(argv[3],"-i")==0)
			{
				iterations=atoi(argv[4]);
			}
		}//if argc==5 close

		if(argc==3)
		{
			if(strcmp(argv[1],"-t")==0)
			{
				num_threads=atoi(argv[2]);
			}

			if(strcmp(argv[1],"-i")==0)
			{
				iterations=atoi(argv[2]);
			}
		}//if argc==3 close

	}//if argc!=0 close

	make_threads_joinsAndReLaunch(num_threads, iterations);
	return 0;
}
