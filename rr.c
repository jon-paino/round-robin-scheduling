#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/queue.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

typedef uint32_t u32;
typedef int32_t i32;

struct process
{
  u32 pid;
  u32 arrival_time;
  u32 burst_time;

  TAILQ_ENTRY(process) pointers;

  /* Additional fields here */
  u32 scheduled_time;
  u32 completion_time;
  u32 execution_time;
  bool been_scheduled;
  /* End of "Additional fields here" */
};

TAILQ_HEAD(process_list, process);

u32 next_int(const char **data, const char *data_end)
{
  u32 current = 0;
  bool started = false;
  while (*data != data_end)
  {
    char c = **data;

    if (c < 0x30 || c > 0x39)
    {
      if (started)
      {
        return current;
      }
    }
    else
    {
      if (!started)
      {
        current = (c - 0x30);
        started = true;
      }
      else
      {
        current *= 10;
        current += (c - 0x30);
      }
    }

    ++(*data);
  }

  printf("Reached end of file while looking for another integer\n");
  exit(EINVAL);
}

u32 next_int_from_c_str(const char *data)
{
  char c;
  u32 i = 0;
  u32 current = 0;
  bool started = false;
  while ((c = data[i++]))
  {
    if (c < 0x30 || c > 0x39)
    {
      exit(EINVAL);
    }
    if (!started)
    {
      current = (c - 0x30);
      started = true;
    }
    else
    {
      current *= 10;
      current += (c - 0x30);
    }
  }
  return current;
}

void init_processes(const char *path,
                    struct process **process_data,
                    u32 *process_size)
{
  int fd = open(path, O_RDONLY);
  if (fd == -1)
  {
    int err = errno;
    perror("open");
    exit(err);
  }

  struct stat st;
  if (fstat(fd, &st) == -1)
  {
    int err = errno;
    perror("stat");
    exit(err);
  }

  u32 size = st.st_size;
  const char *data_start = mmap(NULL, size, PROT_READ, MAP_PRIVATE, fd, 0);
  if (data_start == MAP_FAILED)
  {
    int err = errno;
    perror("mmap");
    exit(err);
  }

  const char *data_end = data_start + size;
  const char *data = data_start;

  *process_size = next_int(&data, data_end);

  *process_data = calloc(sizeof(struct process), *process_size);
  if (*process_data == NULL)
  {
    int err = errno;
    perror("calloc");
    exit(err);
  }

  for (u32 i = 0; i < *process_size; ++i)
  {
    (*process_data)[i].pid = next_int(&data, data_end);
    (*process_data)[i].arrival_time = next_int(&data, data_end);
    (*process_data)[i].burst_time = next_int(&data, data_end);
  }

  munmap((void *)data, size);
  close(fd);
}

bool timeLeft(struct process **data, u32 size) {
  for (int i = 0; i < size; i++) {
    /* If any process still needs time */
    if ((*data)[i].burst_time > 0) {  
      return true;
    }
  }
  return false; 
}

/* While performing a time slice for a given process, determine if any others arrive at the current time and add them to the queue */
void scheduleArrival(struct process **data, struct process *current_process, struct process_list *list, u32 current_time, u32 size){
      for(int i = 0; i < size; i++){
        if (&(*data)[i] == current_process){ 
          continue;
        }
        if((*data)[i].arrival_time == current_time && (*data)[i].burst_time > 0){
          TAILQ_INSERT_TAIL(list, &(*data)[i], pointers);
        }
      }
}

int main(int argc, char *argv[])
{
  if (argc != 3)
  {
    return EINVAL;
  }
  struct process *data;
  u32 size;
  init_processes(argv[1], &data, &size);

  u32 quantum_length = next_int_from_c_str(argv[2]);

  struct process_list list;
  TAILQ_INIT(&list);

  u32 total_waiting_time = 0;
  u32 total_response_time = 0;

  /* Your code here */

  /* Find the first process to be scheduled (quickest arrival time) */
  u32 firstArrival = data[0].arrival_time; 
  u32 firstProcess = 0;
  for (int i = 1; i < size; i++){
    if (data[i].arrival_time < firstArrival){
      firstArrival = data[i].arrival_time;
      firstProcess = i;
    }
  }
  /* Insert the first arriving process onto the queue */
  TAILQ_INSERT_TAIL(&list, &data[firstProcess], pointers);

  /* Round-robin implementation */
  u32 current_time = firstArrival;
  /* While there are still processes to be run */
  while(timeLeft(&data, size)){ 
    struct process *current_process = TAILQ_FIRST(&list);
    if (current_process == NULL) {
      do {
        current_time++;  // Advance time if no process is in the queue
        scheduleArrival(&data, NULL, &list, current_time, size);  // Check for new arrivals
      } while (TAILQ_EMPTY(&list) && timeLeft(&data, size));  // Continue if the list is still empty and there are unfinished processes
      continue;
    }
    /* Start running the process at the front of the queue*/
    TAILQ_REMOVE(&list, current_process, pointers);
    if (!current_process->been_scheduled) {
      current_process->scheduled_time = current_time;
      current_process->been_scheduled = true;
    }
    /* If any other processes have the same arrival time as the first one place them on the queue */
    scheduleArrival(&data, current_process, &list, current_time, size);

    u32 time_slice = quantum_length < current_process->burst_time ? quantum_length : current_process->burst_time;
    for(u32 i = 0; i < time_slice; i++){
        current_time++;
        current_process->execution_time++;
        current_process->burst_time--;
        /* If any others arrive in the time being, add them to the queue */
        scheduleArrival(&data, current_process, &list, current_time, size);
    }
     /* If process finishes, record completion time */
    if (current_process->burst_time == 0) { 
      current_process->completion_time = current_time + 1;
    }else{
      /* Requeue if the process is not completed */
      TAILQ_INSERT_TAIL(&list, current_process, pointers);
    }
  }
  /* Adjust total waiting and response time */
  for (int i = 0; i < size; i++){
    total_waiting_time += (data[i].completion_time - data[i].scheduled_time) - data[i].execution_time;
    total_response_time += (data[i].scheduled_time - data[i].arrival_time);
  }
  /* End of "Your code here" */

  printf("Average waiting time: %.2f\n", (float)total_waiting_time / (float)size);
  printf("Average response time: %.2f\n", (float)total_response_time / (float)size);

  free(data);
  return 0;
}
