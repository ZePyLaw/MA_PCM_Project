#include <cstdlib>
#include <exception>
#include <iostream>

#include "parallel_task_runner.hpp"
#include "tsptask.hpp"

/*****************************************************************
  Program to solve a TSP problem

  Arguments: tsp <filename> [number] [threads]

  The program compares three execution strategies:
   - DirectTaskRunner: sequential recursive resolution.
   - PartitionedTaskStackRunner: sequential task decomposition.
   - ParallelTaskRunner: concurrent task execution.
 
   The best solution is reset between executions.
   All three runners must produce the same optimal distance.
 *****************************************************************/

int main(int argc, char** argv) {
	if (argc < 2 || argc > 4) {
		std::cerr << "Usage: " << argv[0] << " <file.tsp> [number] [threads]\n";
		return 1;
	}

	try {
		TSPGraph graph(argv[1]);
		if (argc >= 3) {
			char* end = nullptr;
			long n = std::strtol(argv[2], &end, 10);
			if (*argv[2] == '\0' || *end != '\0' || n < 1 || n > TSPPath::MAX_GRAPH) {
				std::cerr << "number must be an integer between 1 and "
					  << TSPPath::MAX_GRAPH << "\n";
				return 1;
			}
			graph.resize(static_cast<int>(n));
		}

		int thread_count = 4;

		if (argc == 4) {
		    char* end = nullptr;
		    long n = std::strtol(argv[3], &end, 10);
		
		    if (*argv[3] == '\0' || *end != '\0' ||
		        n < 1 || n > 256) {
		        std::cerr << "threads must be between 1 and 256\n";
		        return 1;
		    }
		
		    thread_count = static_cast<int>(n);
		}

		TSPPath::setup(&graph);

		TSPTask tsp2;
		DirectTaskRunner r2;
		r2.run(&tsp2);
		std::cout << "direct: " << tsp2.result() << " t:" << r2.duration() << std::endl;
		int direct_distance = tsp2.result().distance();

		TSPTask::reset_result(); // the partitioned run must not start with the best path

		TSPTask tsp1;
		tsp1.cutoff(0);
		PartitionedTaskStackRunner r1(TSPPath::MAX_GRAPH);
		r1.run(&tsp1);
		std::cout << "partit: " << tsp1.result() << " t:" << r1.duration()
			  << " s:" << r1.solves() << "/" << r1.splits() << std::endl;

		if (tsp1.result().distance() != direct_distance) {
			std::cerr << "MISMATCH: direct " << direct_distance
				  << " vs partitioned " << tsp1.result().distance() << "\n";
			return 2;
		}

		// Reset the shared best solution before the parallel run.
		TSPTask::reset_result();

		TSPTask tsp_parallel;
		tsp_parallel.cutoff(4);

		ParallelTaskRunner parallel_runner(
		    thread_count,
		    TSPPath::MAX_GRAPH
		);

		parallel_runner.run(&tsp_parallel);

		std::cout << "parallel (" << thread_count << " threads): "
		          << tsp_parallel.result()
		          << " t:" << parallel_runner.duration()
		          << std::endl;

		// Verify that the parallel result matches the sequential one.
		if (tsp_parallel.result().distance() != direct_distance) {
		    std::cerr << "MISMATCH: parallel result differs "
		              << "from sequential result\n";
		    return 2;
		}

	} catch (const std::exception& e) {
		std::cerr << "error: " << e.what() << "\n";
		return 1;
	}
	return 0;
}
