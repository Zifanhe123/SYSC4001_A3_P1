#include "interrupts_101258593.hpp"

const unsigned int TIME_QUANTUM = 100;

// sort ready queue by priority (ascending), then PID
static void sort_ready_EP_RR(std::vector<PCB> &ready_queue) {
    std::sort(
        ready_queue.begin(),
        ready_queue.end(),
        [](const PCB &a, const PCB &b){
            if (a.priority == b.priority)
                return a.PID < b.PID;
            return a.priority < b.priority;   // smaller priority = higher
        }
    );
}

static bool exists_higher_priority(const std::vector<PCB> &ready_queue,
                                   const PCB &running)
{
    for (const auto &p : ready_queue) {
        if (p.priority < running.priority) return true;
    }
    return false;
}

static void execute_one_ms_EP_RR(
        PCB &running,
        std::vector<PCB> &ready_queue,
        std::vector<PCB> &wait_queue,
        std::vector<PCB> &job_list,
        std::string &execution_status,
        unsigned int current_time)
{
    if (running.PID == -1 || running.state != RUNNING) return;

    // always spend 1 ms of CPU
    running.remaining_time--;
    running.time_in_quantum++;

    // Only track I/O if process actually uses I/O
    bool has_io = (running.io_freq > 0 && running.io_duration > 0);
    if (has_io) {
        running.cpu_since_last_io++;

        // I/O request?
        if (running.cpu_since_last_io >= running.io_freq &&
            running.remaining_time > 0)
        {
            states old_state = running.state;
            running.state = WAITING;
            running.io_remaining = running.io_duration;
            running.cpu_since_last_io = 0;
            running.time_in_quantum = 0;

            execution_status += print_exec_status(current_time,
                                                  running.PID,
                                                  old_state,
                                                  running.state);

            wait_queue.push_back(running);
            sync_queue(job_list, running);
            idle_CPU(running);
            return;
        }
    }

    // Finished?
    if (running.remaining_time == 0) {
        states old_state = running.state;
        running.state = TERMINATED;
        terminate_process(running, job_list);

        execution_status += print_exec_status(current_time,
                                              running.PID,
                                              old_state,
                                              running.state);

        idle_CPU(running);
        return;
    }

    // Preempt if there is a READY process with higher priority
    if (exists_higher_priority(ready_queue, running)) {
        states old_state = running.state;
        running.state = READY;
        running.time_in_quantum = 0;

        execution_status += print_exec_status(current_time,
                                              running.PID,
                                              old_state,
                                              running.state);

        ready_queue.push_back(running);
        sync_queue(job_list, running);
        idle_CPU(running);
        return;
    }

    // Preempt by quantum (RR inside same priority level)
    if (running.time_in_quantum >= TIME_QUANTUM) {
        states old_state = running.state;
        running.state = READY;
        running.time_in_quantum = 0;

        execution_status += print_exec_status(current_time,
                                              running.PID,
                                              old_state,
                                              running.state);

        ready_queue.push_back(running);
        sync_queue(job_list, running);
        idle_CPU(running);
        return;
    }

    sync_queue(job_list, running);
}

static void manage_wait_queue(
        std::vector<PCB> &wait_queue,
        std::vector<PCB> &ready_queue,
        std::vector<PCB> &job_list,
        std::string &execution_status,
        unsigned int current_time)
{
    std::vector<PCB> still_waiting;

    for (auto &p : wait_queue) {
        // avoid underflow
        if (p.io_remaining > 0) {
            p.io_remaining--;
        }

        if (p.io_remaining == 0) {
            states old_state = p.state;
            p.state = READY;
            p.time_in_quantum = 0;
            p.cpu_since_last_io = 0;

            execution_status += print_exec_status(current_time,
                                                  p.PID,
                                                  old_state,
                                                  p.state);
            ready_queue.push_back(p);
            sync_queue(job_list, p);
        } else {
            still_waiting.push_back(p);
            sync_queue(job_list, p);
        }
    }

    wait_queue.swap(still_waiting);
}

static void dispatch_EP_RR(
        PCB &running,
        std::vector<PCB> &ready_queue,
        std::vector<PCB> &job_list,
        std::string &execution_status,
        unsigned int current_time)
{
    if (running.PID != -1 && running.state == RUNNING) return;
    if (ready_queue.empty()) return;

    sort_ready_EP_RR(ready_queue);
    PCB next = ready_queue.front();        // highest priority after sort
    ready_queue.erase(ready_queue.begin());

    states old_state = next.state;
    next.state = RUNNING;
    next.time_in_quantum = 0;
    if (next.start_time == -1)
        next.start_time = current_time;

    execution_status += print_exec_status(current_time,
                                          next.PID,
                                          old_state,
                                          next.state);

    running = next;
    sync_queue(job_list, running);
}

//---------------------------------------------------------------------
// Main simulation for EP + RR
//---------------------------------------------------------------------

std::tuple<std::string> run_simulation(std::vector<PCB> list_processes) {

    std::vector<PCB> ready_queue;
    std::vector<PCB> wait_queue;
    std::vector<PCB> job_list;

    unsigned int current_time = 0;
    PCB running;
    idle_CPU(running);

    std::string execution_status = print_exec_header();

    while (!all_process_terminated(job_list) || job_list.empty()) {

        // Admit processes whose arrival_time <= current_time
        for (auto &process : list_processes) {
            if (process.arrival_time <= current_time &&
                process.state == NOT_ASSIGNED) {

                if (assign_memory(process)) {
                    process.state = READY;
                    process.cpu_since_last_io = 0;
                    process.io_remaining = 0;
                    process.time_in_quantum = 0;

                    ready_queue.push_back(process);
                    job_list.push_back(process);

                    execution_status += print_exec_status(current_time,
                                                          process.PID,
                                                          NEW,
                                                          READY);
                }
            }
        }

        // WAITING -> READY
        manage_wait_queue(wait_queue, ready_queue, job_list,
                          execution_status, current_time);

        // CPU step (handles I/O, completion, preemption)
        execute_one_ms_EP_RR(running, ready_queue, wait_queue,
                             job_list, execution_status, current_time);

        // If CPU idle, pick next by priority + RR
        dispatch_EP_RR(running, ready_queue, job_list,
                       execution_status, current_time);

        current_time++;
    }

    execution_status += print_exec_footer();
    return std::make_tuple(std::move(execution_status));
}

int main(int argc, char** argv) {

    if(argc != 2) {
        std::cout << "ERROR!\nExpected 1 argument, received "
                  << argc - 1 << std::endl;
        std::cout << "To run the program, do: ./interrupts_EP_RR_101258593 "
                     "<input_file.txt>" << std::endl;
        return -1;
    }

    auto file_name = argv[1];
    std::ifstream input_file(file_name);
    if (!input_file.is_open()) {
        std::cerr << "Error: Unable to open file: "
                  << file_name << std::endl;
        return -1;
    }

    std::string line;
    std::vector<PCB> list_process;
    while(std::getline(input_file, line)) {
        auto input_tokens = split_delim(line, ", ");
        auto new_process = add_process(input_tokens);
        list_process.push_back(new_process);
    }
    input_file.close();

    auto [exec] = run_simulation(list_process);
    write_output(exec, "output_files/execution_EP_RR.txt");

    return 0;
}
