//
//  selection.cpp
//
//
//  Created by Thijs Janzen on 28/02/2018.
//
//

#include <iostream>
#include <vector>
#include <fstream>
#include <cstdlib>
#include <numeric>
#include <cmath>

#include <vector>
#include <algorithm>

#include "Fish.h"
#include "random_functions.h"
#include "helper_functions.h"

#include <RcppArmadillo.h>
// [[Rcpp::depends("RcppArmadillo")]]
using namespace Rcpp;

std::vector< Fish > simulate_Population(const std::vector< Fish>& sourcePop,
                                        const NumericMatrix& select,
                                        int pop_size,
                                        int total_runtime,
                                        double morgan,
                                        bool progress_bar,
                                        arma::mat& frequencies,
                                        bool track_frequency,
                                        const NumericVector& track_markers,
                                        bool track_junctions,
                                        std::vector<double>& junctions,
                                        bool multiplicative_selection,
                                        int num_alleles,
                                        const std::vector<int>& founder_labels) {

    //Rcout << "simulate_population: " << multiplicative_selection << "\n";

    bool use_selection = FALSE;
    if(select(1, 1) >= 0) use_selection = TRUE;

    double expected_max_fitness = 1e-6;
    std::vector<Fish> Pop = sourcePop;
    std::vector<double> fitness;
    double maxFitness = -1;

    if(use_selection) {
        for(int j = 0; j < select.nrow(); ++j) {
            if(select(j, 4) < 0) break; // these entries are only for tracking, not for selection calculations
            double local_max_fitness = 0.0;
            for(int i = 1; i < 4; ++i) {
                if(select(j, i) > local_max_fitness) {
                    local_max_fitness = select(j, i);
                }
            }
            expected_max_fitness += local_max_fitness;
        }

        for(auto it = Pop.begin(); it != Pop.end(); ++it){
            double fit = calculate_fitness((*it), select, multiplicative_selection);
            if(fit > maxFitness) maxFitness = fit;

            if(fit > (expected_max_fitness)) { // little fix to avoid numerical problems
                Rcout << "Expected maximum " << expected_max_fitness << " found " << fit << "\n";
                Rcpp::stop("ERROR in calculating fitness, fitness too large\n");
            }

            fitness.push_back(fit);
        }
    }

    int updateFreq = total_runtime / 20;
    if(updateFreq < 1) updateFreq = 1;

    if(progress_bar) {
        Rcout << "0--------25--------50--------75--------100\n";
        Rcout << "*";
    }

    for(int t = 0; t < total_runtime; ++t) {

        if(track_junctions) junctions.push_back(calc_mean_junctions(Pop));

        if(track_frequency) {
            //Rcout << "track frequency start\n";
            for(int i = 0; i < track_markers.size(); ++i) {
                if(track_markers[i] < 0) break;
                arma::mat local_mat = update_frequency_tibble(Pop,
                                                              track_markers[i],
                                                              founder_labels,
                                                              t);

                // now we have to find where to copy local_mat into frequencies
                int time_block = track_markers.size() * founder_labels.size(); // number of markers times number of alleles

                int start_add_time = t * time_block;
                int start_add_marker = i * founder_labels.size() + start_add_time;

                for(int j = 0; j < founder_labels.size(); ++j) {
                    for(int k = 0; k < 4; ++k) {
                       frequencies(start_add_marker + j, k)  = local_mat(j, k);
                    }
                }
            }
        }

        std::vector<Fish> newGeneration;
        std::vector<double> newFitness;
        double newMaxFitness = -1.0;
        //Rcout << "updating fish\n";
        for(int i = 0; i < pop_size; ++i)  {
            int index1 = 0;
            int index2 = 0;
            if(use_selection) {
                index1 = draw_prop_fitness(fitness, maxFitness);
                index2 = draw_prop_fitness(fitness, maxFitness);
                while(index2 == index1) index2 = draw_prop_fitness(fitness, maxFitness);
            } else {
                index1 = random_number( (int)Pop.size() );
                index2 = random_number( (int)Pop.size() );
                while(index2 == index1) index2 = random_number( (int)Pop.size() );
            }

            Fish kid = mate(Pop[index1], Pop[index2], morgan);

            newGeneration.push_back(kid);

            double fit = -2.0;
            if(use_selection) fit = calculate_fitness(kid, select, multiplicative_selection);
            if(fit > newMaxFitness) newMaxFitness = fit;

            if(fit > expected_max_fitness) {
                Rcout << "Expected maximum " << expected_max_fitness << " found " << fit << "\n";
                Rcpp::stop("ERROR in calculating fitness, fitness too large\n");
            }

            newFitness.push_back(fit);
        }

        if(t % updateFreq == 0 && progress_bar) {
            Rcout << "**";
        }

        if(is_fixed(Pop)) {
            Rcout << "\n After " << t << " generations, the population has become completely homozygous and fixed\n";
            R_FlushConsole();
            return(Pop);
        }

        Rcpp::checkUserInterrupt();

        Pop = newGeneration;
        newGeneration.clear();
        fitness = newFitness;
        maxFitness = newMaxFitness;
   //     Rcout << "done updating, again!\n";
    }
    if(progress_bar) Rcout << "\n";
    return(Pop);
}

// [[Rcpp::export]]
List simulate_cpp(Rcpp::NumericVector input_population,
              NumericMatrix select,
              int pop_size,
              int number_of_founders,
              Rcpp::NumericVector starting_proportions,
              int total_runtime,
              double morgan,
              bool progress_bar,
              bool track_frequency,
              NumericVector track_markers,
              bool track_junctions,
              bool multiplicative_selection)
{
    //Rcout << "simulate_cpp: " << multiplicative_selection << "\n";

    std::vector< Fish > Pop;
    int number_of_alleles = number_of_founders;
    std::vector<int> founder_labels;

    if(input_population[0] > -1e4) {
        //Rcout << "Found input population! converting!\n";
        Pop = convert_NumericVector_to_fishVector(input_population);

        number_of_founders = 0;
        for(auto it = Pop.begin(); it != Pop.end(); ++it) {
            update_founder_labels((*it).chromosome1, founder_labels);
            update_founder_labels((*it).chromosome2, founder_labels);
        }
        number_of_alleles = founder_labels.size();
        //Rcout << "Number of alleles is " << number_of_alleles << "\n";
    } else {
        std::vector<double> starting_freqs = as< std::vector<double> >(starting_proportions);
        for(int i = 0; i < pop_size; ++i) {
            int founder_1 = draw_random_founder(starting_freqs);
            int founder_2 = draw_random_founder(starting_freqs);

            Fish p1 = Fish( founder_1 );
            Fish p2 = Fish( founder_2 );

            Pop.push_back(mate(p1,p2, morgan));
        }
        for(int i = 0; i < number_of_alleles; ++i) {
            founder_labels.push_back(i);
        }
    }

    arma::mat frequencies_table;

    if(track_frequency) {
        //Rcout << "Preparing frequencies_table\n";
       // int number_entries = track_markers.size();
        int number_of_markers = track_markers.size();
        //arma::cube x(total_runtime, number_of_alleles, number_entries); // n_row, n_col, n_slices, type
        arma::mat x(number_of_markers * number_of_alleles * total_runtime, 4); // 4 columns: time, loc, anc, type
        frequencies_table = x;
    }

    arma::mat initial_frequencies = update_all_frequencies_tibble(Pop, track_markers, founder_labels, 0);
  //  Rcout << "collected initial frequencies\n";

    std::vector<double> junctions;
  //  Rcout << "starting simulation\n";
    std::vector<Fish> outputPop = simulate_Population(Pop,
                                                      select,
                                                      pop_size,
                                                      total_runtime,
                                                      morgan,
                                                      progress_bar,
                                                      frequencies_table,
                                                      track_frequency,
                                                      track_markers,
                                                      track_junctions,
                                                      junctions,
                                                      multiplicative_selection,
                                                      number_of_alleles,
                                                      founder_labels);
    //Rcout << "finished simulation\n";
    arma::mat final_frequencies = update_all_frequencies_tibble(outputPop, track_markers, founder_labels, total_runtime);

    return List::create( Named("population") = convert_to_list(outputPop),
                         Named("frequencies") = frequencies_table,
                         Named("initial_frequencies") = initial_frequencies,
                         Named("final_frequencies") = final_frequencies,
                         Named("junctions") = junctions);
}



std::vector< Fish > next_pop_migr(const std::vector< Fish>& pop_1,
                                  const std::vector< Fish>& pop_2,
                                  int pop_size,
                                  std::vector< double > fitness_source,
                                  std::vector< double > fitness_migr,
                                  double max_fitness_source,
                                  double max_fitness_migr,
                                  const NumericMatrix& select,
                                  bool use_selection,
                                  bool multiplicative_selection,
                                  double migration_rate,
                                  std::vector< double >& new_fitness,
                                  double& new_max_fitness,
                                  double size_in_morgan) {

    std::vector<Fish> new_generation;
    new_fitness.clear();
    new_max_fitness = -1.0;
    for(int i = 0; i < pop_size; ++i)  {
        Fish parent1;
        Fish parent2;

        if(uniform() < migration_rate) {
            // migration
            if(use_selection)  {
                parent1 = pop_2[ draw_prop_fitness(fitness_migr, max_fitness_migr) ];
            } else {
                parent1 = pop_2[ random_number( (int)pop_1.size() ) ];
            }
        } else {
            if(use_selection)  {
                parent1 = pop_1[ draw_prop_fitness(fitness_source, max_fitness_source) ];
            } else {
                parent1 = pop_1[ random_number( (int)pop_1.size() ) ];
            }
        }

       if(uniform() < migration_rate) {
           if(use_selection) {
               parent2 = pop_2[ draw_prop_fitness(fitness_migr, max_fitness_migr) ];
               while(parent1 == parent2) parent2 = pop_2[ draw_prop_fitness(fitness_migr, max_fitness_migr) ];
           } else {
               parent2 = pop_2[ random_number( (int)pop_1.size()  )];
               while(parent1 == parent2) pop_2[ random_number( (int)pop_1.size() )];
           }
      } else {
          if(use_selection)  {
              parent1 = pop_1[ draw_prop_fitness(fitness_source, max_fitness_source) ];
               while(parent1 == parent2) parent1 = pop_1[ draw_prop_fitness(fitness_source, max_fitness_source) ];
          } else {
              parent1 = pop_1[ random_number( (int)pop_1.size() )];
              while(parent1 == parent2) parent1 = pop_1[ random_number( (int)pop_1.size() )];
          }
      }

      Fish kid = mate(parent1, parent2, size_in_morgan);

      new_generation.push_back(kid);

      double fit = -2.0;
      if(use_selection) fit = calculate_fitness(kid, select, multiplicative_selection);
      if(fit > new_max_fitness) new_max_fitness = fit;

      new_fitness.push_back(fit);
    }
    return new_generation;
}


std::vector< std::vector< Fish >> simulate_two_populations(const std::vector< Fish>& source_pop_1,
                                             const std::vector< Fish>& source_pop_2,
                                             const NumericMatrix& select,
                                             const NumericVector& pop_size,
                                             int total_runtime,
                                             double morgan,
                                             bool progress_bar,
                                             arma::mat& frequencies,
                                             bool track_frequency,
                                             const NumericVector& track_markers,
                                             bool track_junctions,
                                             std::vector<double>& junctions,
                                             bool multiplicative_selection,
                                             int num_alleles,
                                             const std::vector<int>& founder_labels,
                                             double migration_rate) {

    //Rcout << "simulate_population: " << multiplicative_selection << "\n";

    bool use_selection = FALSE;
    if(select(1, 1) >= 0) use_selection = TRUE;

    double expected_max_fitness = 1e-6;
    std::vector<Fish> pop_1 = source_pop_1;
    std::vector<Fish> pop_2 = source_pop_2;

    double max_fitness_pop_1, max_fitness_pop_2;
    std::vector<double> fitness_pop_1, fitness_pop_2;

    if(use_selection) {
        for(int j = 0; j < select.nrow(); ++j) {
            if(select(j, 4) < 0) break; // these entries are only for tracking, not for selection calculations
            double local_max_fitness = 0.0;
            for(int i = 1; i < 4; ++i) {
                if(select(j, i) > local_max_fitness) {
                    local_max_fitness = select(j, i);
                }
            }
            expected_max_fitness += local_max_fitness;
        }

        for(auto it = pop_1.begin(); it != pop_1.end(); ++it){
            double fit = calculate_fitness((*it), select, multiplicative_selection);
            if(fit > max_fitness_pop_1) max_fitness_pop_1 = fit;

            if(fit > (expected_max_fitness)) { // little fix to avoid numerical problems
                Rcout << "Expected maximum " << expected_max_fitness << " found " << fit << "\n";
                Rcpp::stop("ERROR in calculating fitness, fitness too large\n");
            }

            fitness_pop_1.push_back(fit);
        }

        for(auto it = pop_2.begin(); it != pop_2.end(); ++it){
            double fit = calculate_fitness((*it), select, multiplicative_selection);
            if(fit > max_fitness_pop_2) max_fitness_pop_2 = fit;

            if(fit > (expected_max_fitness)) { // little fix to avoid numerical problems
                Rcout << "Expected maximum " << expected_max_fitness << " found " << fit << "\n";
                Rcpp::stop("ERROR in calculating fitness, fitness too large\n");
            }

            fitness_pop_2.push_back(fit);
        }
    }

    int updateFreq = total_runtime / 20;
    if(updateFreq < 1) updateFreq = 1;

    if(progress_bar) {
        Rcout << "0--------25--------50--------75--------100\n";
        Rcout << "*";
    }

    for(int t = 0; t < total_runtime; ++t) {

       // if(track_junctions) junctions.push_back(calc_mean_junctions(Pop));

        if(track_frequency) {
            //Rcout << "track frequency start\n";
            for(int i = 0; i < track_markers.size(); ++i) {
                if(track_markers[i] < 0) break;

                arma::mat local_mat = update_all_frequencies_tibble_dual_pop (pop_1, pop_2,
                                                              track_markers[i],
                                                              founder_labels,
                                                              t);

                // now we have to find where to copy local_mat into frequencies
                int time_block = track_markers.size() * founder_labels.size() * 2; // number of markers times number of alleles times number of populations

                int start_add_time = t * time_block;
                int start_add_marker = i * founder_labels.size() + start_add_time;

                for(int j = 0; j < founder_labels.size(); ++j) {
                    for(int k = 0; k < 4; ++k) {
                        frequencies(start_add_marker + j, k)  = local_mat(j, k);
                    }
                }
            }
        }

        double new_max_fitness_pop_1, new_max_fitness_pop_2;
        std::vector<double> new_fitness_pop_1, new_fitness_pop_2;

        std::vector<Fish> new_generation_pop_1 = next_pop_migr(pop_1,      pop_2, pop_size[0],
                                                               fitness_pop_1, fitness_pop_2,
                                                               max_fitness_pop_1, mmax_fitness_pop_2,
                                                               select, use_selection, multiplicative_selection,
                                                               migration_rate, new_fitness_pop_1, new_max_fitness_pop_1,
                                                               morgan);

        std::vector<Fish> new_generation_pop_2 = next_pop_migr(pop_2,      pop_1, pop_size[1],
                                                               fitness_pop_2, fitness_pop_1,
                                                               max_fitness_pop_2, max_fitness_pop_1,
                                                               select, use_selection, multiplicative_selection,
                                                               migration_rate, new_fitness_pop_2, new_max_fitness_pop_2,
                                                               morgan);

        pop_1 = new_generation_pop_1;
        pop_2 = new_generation_pop_2;
        fitness_pop_1 = new_fitness_pop_1;
        fitness_pop_2 = new_fitness_pop_2;
        max_fitness_pop_1 = new_max_fitness_pop_1;
        max_fitness_pop_2 = new_max_fitness_pop_2;

        if(t % updateFreq == 0 && progress_bar) {
            Rcout << "**";
        }

        if(is_fixed(pop_1) && is_fixed(pop_2)) {
            Rcout << "\n After " << t << " generations, the population has become completely homozygous and fixed\n";
            R_FlushConsole();
            std::vector< std::vector < Fish > > output;
            output.push_back(pop_1);
            output.push_back(pop_2);
            return(output);
        }

        Rcpp::checkUserInterrupt();
    }
    if(progress_bar) Rcout << "\n";
    std::vector< std::vector< Fish >> output;
    output.push_back(pop_1);
    output.push_back(pop_2);
    return(output);
}





// [[Rcpp::export]]
List simulate_migration_cpp(Rcpp::NumericVector input_population_1,
                            Rcpp::NumericVector input_population_2,
                            NumericMatrix select,
                            Rcpp::NumericVector pop_size,
                            int number_of_founders,
                            Rcpp::NumericVector starting_proportions,
                            int total_runtime,
                            double morgan,
                            bool progress_bar,
                            bool track_frequency,
                            NumericVector track_markers,
                            bool track_junctions,
                            bool multiplicative_selection,
                            double migration_rate)
{
    //Rcout << "simulate_cpp: " << multiplicative_selection << "\n";

    std::vector< Fish > Pop_1;
    std::vector< Fish > Pop_2;
    int number_of_alleles = number_of_founders;
    std::vector<int> founder_labels;

    if(input_population_1[0] > -1e4) {
        //Rcout << "Found input populations! converting!\n";
        Pop_1 = convert_NumericVector_to_fishVector(input_population_1);
        Pop_2 = convert_NumericVector_to_fishVector(input_population_2);

        number_of_founders = 0;
        for(auto it = Pop_1.begin(); it != Pop_1.end(); ++it) {
            update_founder_labels((*it).chromosome1, founder_labels);
            update_founder_labels((*it).chromosome2, founder_labels);
        }

        for(auto it = Pop_2.begin(); it != Pop_2.end(); ++it) {
            update_founder_labels((*it).chromosome1, founder_labels);
            update_founder_labels((*it).chromosome2, founder_labels);
        }

        number_of_alleles = founder_labels.size();
        //Rcout << "Number of alleles is " << number_of_alleles << "\n";
    } else {
        std::vector<double> starting_freqs = as< std::vector<double> >(starting_proportions);

        for(int j = 0; j < 2; ++j) {
            for(int i = 0; i < pop_size[j]; ++i) {
                int founder_1 = draw_random_founder(starting_freqs);
                int founder_2 = draw_random_founder(starting_freqs);

                Fish p1 = Fish( founder_1 );
                Fish p2 = Fish( founder_2 );

                if(j == 0) Pop_1.push_back(mate(p1,p2, morgan));
                if(j == 1) Pop_2.push_back(mate(p1,p2, morgan));
            }
            for(int i = 0; i < number_of_alleles; ++i) {
                founder_labels.push_back(i);
            }
        }
    }

    arma::mat frequencies_table;

    if(track_frequency) {
        //Rcout << "Preparing frequencies_table\n";
        int number_of_markers = track_markers.size();
        //arma::cube x(total_runtime, number_of_alleles, number_entries); // n_row, n_col, n_slices, type
        arma::mat x(number_of_markers * number_of_alleles * total_runtime, 5); // 4 columns: time, loc, anc, type, population
        frequencies_table = x;
    }

    arma::mat initial_frequencies = update_all_frequencies_tibble_dual_pop(Pop_1, Pop_2, track_markers, founder_labels, 0);
    //  Rcout << "collected initial frequencies\n";

    std::vector<double> junctions;
    //  Rcout << "starting simulation\n";
    std::vector< std::vector< Fish> > output_populations = simulate_two_populations(Pop_1,
                                                                                Pop_2,
                                                                                select,
                                                                                pop_size,
                                                                                total_runtime,
                                                                                morgan,
                                                                                progress_bar,
                                                                                frequencies_table,
                                                                                track_frequency,
                                                                                track_markers,
                                                                                track_junctions,
                                                                                junctions,
                                                                                multiplicative_selection,
                                                                                number_of_alleles,
                                                                                founder_labels,
                                                                                migration_rate);
    //Rcout << "finished simulation\n";
    arma::mat final_frequencies = update_all_frequencies_tibble_dual_pop(output_populations[0],
                                                                         output_populations[1],
                                                                         track_markers, founder_labels, total_runtime);

    return List::create( Named("population_1") = convert_to_list(output_populations[0]),
                         Named("population_2") = convert_to_list(output_populations[1]),
                         Named("frequencies") = frequencies_table,
                         Named("initial_frequencies") = initial_frequencies,
                         Named("final_frequencies") = final_frequencies,
                         Named("junctions") = junctions);
}



// [[Rcpp::export]]
void test_fish_functions() {
    Fish test_fish;

    junction temp;
    junction temp2(0.5, 0);
    junction temp3(0.5, 0);
    if(temp2 == temp3) {
        temp = temp2;
    }

    bool temp400 = (temp2 != temp3);
    Rcout << temp400 << "\t" << "this is only for testing\n";

    junction temp4(temp);

    test_fish.chromosome1.push_back(temp);
    test_fish.chromosome1.push_back(temp2);
    test_fish.chromosome1.push_back(temp3);
    test_fish.chromosome1.push_back(temp4);

    Fish test_fish2 = test_fish;

    if(test_fish == test_fish2) {
        Rcout << "fishes are equal!\n";
    }

    Fish test_fish3(5);
    bool b = (test_fish == test_fish3);
    Rcout << b << "\t" << "this is only for testing\n";

    std::vector< junction > chrom;
    chrom.push_back(temp);

    Fish test_fish4(chrom, chrom);

    std::vector< Fish > pop;
    pop.push_back(test_fish);
    pop.push_back(test_fish2);
    pop.push_back(test_fish3);
    pop.push_back(test_fish4);

    verify_pop_cpp(pop);


    return;
}
