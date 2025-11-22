#include "interrupts_101258593.hpp"

// External Priority: smaller priority value = higher priority
static void EP_schedule(std::vector<PCB> &ready_queue) {
    std::sort(
        ready_queue.begin(),
        ready_queue.end(),
        [](const PCB &a, const PCB &b){
            return a.priority < b.priority;   // ascending
        }
    );
}

// One CPU ms for EP (no quantum preemption)
static void execute_one_ms_EP(
        PCB &running,
        std::vector<PCB> &wait_queue,
        std::vector<PCB> &job_list,
        std::string &execution_status,
        unsigned int current_time)
{
    if (running.PID == -1 || running.state != RUNNING) return;

    // Spend 1 ms of CPU
    running.remaining_time--;

    // Only track I/O if this process actually uses I/O
    bool has_io = (running.io_freq > 0 && running.io_duration > 0);
    if (has_io) {
        running.cpu_since_last_io++;
    }

    // I/O request? (only if io_freq > 0 and io_duration > 0)
    if (has_io &&
        running.cpu_since_last_io >= running.io_freq &&
        running.remaining_time > 0)
    {
        states old_state = running.state;
        running.state = WAITING;
        running.io_remaining = running.io_duration;
        running.cpu_since_last_io = 0;
        running.time_in_quantum = 0;   // not used by EP, but keep clean

        execution_status += print_exec_status(current_time,
                                              running.PID,
                                              old_state,
                                              running.state);

        wait_queue.push_back(running);
        sync_queue(job_list, running);
        idle_CPU(running);
        return;
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

    sync_queue(job_list, running);
}

// Manage WAIT queue: I/O completion
static void manage_wait_queue(
        std::vector<PCB> &wait_queue,
        std::vector<PCB> &ready_queue,
        std::vector<PCB> &job_list,
        std::string &execution_status,
        unsigned int current_time)
{
    std::vector<PCB> still_waiting;

    for (auto &p : wait_queue) {
        // avoid going below 0
        if (p.io_remaining > 0) {
            p.io_remaining--;
        }

        if (p.io_remaining == 0) {
            states old_state = p.state;
            p.state = READY;
            p.time_in_quantum = 0;
            p.cpu_since_last_io = 0;   // reset for next I/O cycle

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

// Dispatch according to External Priority (only if CPU idle)
static void dispatch_EP(
        PCB &running,
        std::vector<PCB> &ready_queue,
        std::vector<PCB> &job_list,
        std::string &execution_status,
        unsigned int current_time)
{
    if (running.PID != -1 && running.state == RUNNING) return;
    if (ready_queue.empty()) return;

    EP_schedule(ready_queue);               // order by priority (ascending)
    PCB next = ready_queue.front();         // highest priority
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
// Main simulation for EP
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

        // 1) Admit processes whose arrival_time <= current_time
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
                // if memory not available, keep NOT_ASSIGNED and try later
            }
        }

        // 2) Manage WAITING -> READY
        manage_wait_queue(wait_queue, ready_queue, job_list,
                          execution_status, current_time);

        // 3) Execute one ms on CPU (if any running)
        execute_one_ms_EP(running, wait_queue, job_list,
                          execution_status, current_time);

        // 4) If CPU idle, dispatch next by priority
        dispatch_EP(running, ready_queue, job_list,
                    execution_status, current_time);

        // 5) Advance simulated time
        current_time++;
    }

    execution_status += print_exec_footer();
    return std::make_tuple(execution_status);
}

int main(int argc, char** argv) {

    if(argc != 2) {
        std::cout << "ERROR!\nExpected 1 argument, received "
                  << argc - 1 << std::endl;
        std::cout << "To run the program, do: ./interrupts_EP_101258593 "
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
    write_output(exec, "output_files/execution_EP.txt");

    return 0;
}
