//#include <stdio.h>
#include <cstdio>
#include <cstdlib>
#include <new>
#include <omp.h>
#include <algorithm>
#include <assert.h>

using namespace std;

// ---------------------------------------------------------------------------------
// Define Lawn_Class
// Variables:
//  Lawn - Represents a lawn that has been divided into a 2D grid of cells (like a 
//         graphing sheet); Lawn is a 2D array where L[i][j] holds the expected 
//         number of ants 
//
typedef struct
    {
        double val;
        int loc1;
        int loc2;
        char pad[128];
    } tvals;
class Lawn_Class {
    public:
	// Lawn query functions
	double number_of_ants_in_cell (int, int);
	int guess_anthill_location (int, int);
	void report_results(double); 
	// Lawn functions
	void initialize_Lawn (int, int, int, int);
	void update_Lawn ();
	// Helper functions
	double sum_Lawn ();
	void print_Lawn ();
	void print_XY ();
	void save_Lawn_to_file ();
    void anthillFinder ();

	int m;			// Number of cells along each dimension of the Lawn

    private:
	int n;			// Total number of cells = m x m
	int anthill_x;		// Anthill is located at (anthill_x,anthill_y)
	int anthill_y;		// in the Lawn
	int * X;		// Cell k is located at (X[k],Y[k])
	int * Y;		
	double ** Lawn;		// Stores number of ants per cell 
	double ** Lawn2;	// Work array used when updating Lawn
	int query_count;	// Number of queries to number_of_ants_in_cell
	int guess_count;	// Number of guesses for anthill location
	int steps;		// Number of steps of simulation
	double penalty;		// Time used to penalize guesses and queries
	double time_to_find_anthill;	// Time needed to find anthill
	// Lawn functions
	void set_anthill_location (int, int);
	void add_ant_to_Lawn ();
	void swap_Lawn_with_Lawn2 ();
};
// ---------------------------------------------------------------------------------
// Returns the expected number of ants in cell (i,j) of the Lawn
//  - increments query_count
double Lawn_Class::number_of_ants_in_cell ( int i, int j) {
#pragma omp atomic update
    query_count++;

    return Lawn[i][j];
}
// ---------------------------------------------------------------------------------
// Determines if the guess that the anthill is at cell (,j,) is true or false
//  - increments guess_count
int Lawn_Class::guess_anthill_location ( int i, int j) {
#pragma omp atomic update
    guess_count++;

    if ((i == anthill_x) && (j == anthill_y)) {
	return 1; 
    } else {
	return 0; 
    }
}
// ---------------------------------------------------------------------------------
// Report results 
void Lawn_Class::report_results (double execution_time) {

    printf("Size = %d x %d, Ants = %8.1f, Queries = %d, Guesses = %d, Penalty = %f, Execution time = %f\n", 
	    m, m, sum_Lawn(), query_count, guess_count, penalty, execution_time); 
}
// ---------------------------------------------------------------------------------
// Initialize Lawn variables
// - size = number of cells along each dimenstion of the Lawn
void Lawn_Class::initialize_Lawn (int side, int anthill_x, int anthill_y, int nsteps) {

    // Initialize Lawn sizes
    m = side;		// Number of cells along each dimension of the Lawn
    n = m * m;		// Total number of cells = m x m

    // Initialize X and Y arrays
    X = new int[n];
    Y = new int[n];
    int idx = 0;
    for (int i = 0; i < m; i++) {
	for (int j = 0; j < m; j++) {
	    X[idx] = i; 
	    Y[idx] = j; 
	    idx++;
	}
    }
    // Set up Lawn as a m x m 2D array from 1D array of length n = m*m
    Lawn = new double *[m]; 
    double * array = new double[n];
    for (int i = 0; i < m; i++) Lawn[i] = &(array[i*m]);

    // Set up Lawn2 similar to Lawn; to be used as a work array when updating Lawn
    Lawn2 = new double *[m]; 
    array = new double[n];
    for (int i = 0; i < m; i++) Lawn2[i] = &(array[i*m]);

    // Initialize Lawn cells with zero ants
    for (int i = 0; i < m; i++) {
	for (int j = 0; j < m; j++) {
	    Lawn[i][j] = 0.0;
	}
    }
    // Place anthill in the lawn at cell (anthill_x,anthill_y) 
    set_anthill_location (anthill_x,anthill_y);

    // Update the lawn steps number of times
    steps = nsteps;
    double time_start = omp_get_wtime();
    for (int s = 0; s < steps; s++) {
	update_Lawn(); 
    }
    // Initialize penalty equal to update_Lawn time multiplied by steps
    penalty = omp_get_wtime() - time_start;

    time_to_find_anthill = 0.0;	// Set time_to_find_anthill to zero
    query_count = 0;	// Set number of queries made to zero
    guess_count = 0;	// Set number of guesses made to zero
}
// ---------------------------------------------------------------------------------
// Update Lawn every time step by calculating expected number of ants in each cell. 
// - Lawn is "renamed" Lawn2 so that Lawn2 has current expected ant values
// - Expected ant values for next time step will be computed from Lawn2 
//   and placed in Lawn, as shown below:
//   Lawn[i][j] = 0.25*(Lawn2[i-1][j]+Lawn2[i+1][j]+Lawn2[i][j-1]+Lawn2[i][j+1])
// - Non-existent neighbors of cell (i,j) have zero contribution 
void Lawn_Class::update_Lawn () {
    int i, j;
    // Rename Lawn as Lawn2, Lawn2 as Lawn
    swap_Lawn_with_Lawn2(); 

    // Update Lawn from Lawn2 values
    double time_start = omp_get_wtime();
#pragma omp parallel for
    for (i = 0; i < m; i++) {
	for (j = 0; j < m; j++) {
	    Lawn[i][j] = 0.0;
	    if (i > 0) Lawn[i][j] += 0.25 * Lawn2[i-1][j]; 
	    if (i < m-1) Lawn[i][j] += 0.25 * Lawn2[i+1][j]; 
	    if (j > 0) Lawn[i][j] += 0.25 * Lawn2[i][j-1]; 
	    if (j < m-1) Lawn[i][j] += 0.25 * Lawn2[i][j+1]; 
	}
    }
    // Add ant to Lawn
    add_ant_to_Lawn(); 
}
// ---------------------------------------------------------------------------------
// Place anthill at cell (x,y) as part of initialization
void Lawn_Class::set_anthill_location (int x, int y) {
    anthill_x = x;
    anthill_y = y;
    if (x < 0) anthill_x = 0;  
    else if (x >= m) anthill_x = m-1;  

    if (y < 0) anthill_y = 0;  
    else if (y >= m) anthill_y = m-1;  
}
// ---------------------------------------------------------------------------------
// Add an ant to the anthill
void Lawn_Class::add_ant_to_Lawn () {
    Lawn[anthill_x][anthill_y] += 1.0;
}
// ---------------------------------------------------------------------------------
// Swap Lawn with Lawn2 (Lawn2 is used as workspace)
void Lawn_Class::swap_Lawn_with_Lawn2 () {
    double ** temp = Lawn; // Replace Lawn 2 with Lawn to fix bug
    Lawn = Lawn2; 
    Lawn2 = temp;
}
// ---------------------------------------------------------------------------------
// Add up all the values in Lawn, which gives the expected number of ants 
// in the Lawn; after sufficient number of time steps when the anyt population 
// reaches an equilibrium, this value does not change with time steps.
double Lawn_Class::sum_Lawn () {
    int i, j;
    double sum = 0.0;
    for (i = 0; i < m; i++) {
	for (j = 0; j < m; j++) {
	    sum += Lawn[i][j]; 
	}
    }
    return sum; 
}
// ---------------------------------------------------------------------------------
// Print Lawn, for debugging purposes
// - Lawn[i][j] is the expected number of ants in cell(i,j)
void Lawn_Class::print_Lawn () {
    int i, j;
    for (i = 0; i < m; i++) {
	printf("Row = %d: ", i); 
	for (j = 0; j < m; j++) {
	    printf("%8.4f", Lawn[i][j]); 
	}
	printf("\n"); 
    }
}
// ---------------------------------------------------------------------------------
// Print X and Y array, for debugging purposes
// - Cell k is at (X[k],Y[k])
void Lawn_Class::print_XY () {
    int i, j;
    int idx = 0;
    for (i = 0; i < m; i++) {
	printf("Row = %d: ", i); 
	for (j = 0; j < m; j++) {
	    printf("(%d,%d) \t", X[idx], Y[idx]); 
	    idx++;
	}
	printf("\n"); 
    }
}
// ---------------------------------------------------------------------------------
// Save Lawn to file for visualization via MATLAB
void Lawn_Class::save_Lawn_to_file () {
    int i, j;
    int idx = 0;
    FILE *outfile; 
    outfile = fopen("Lawn_Data", "w"); 
    for (i = 0; i < m; i++) {
	for (j = 0; j < m; j++) {
	    fprintf(outfile, "%d %d %f\n", X[idx], Y[idx], Lawn[i][j]); 
	    idx++;
	}
    }
    fclose(outfile); 
}
// ---------------------------------------------------------------------------------
// Main program to find the anthill
// 
void Lawn_Class::anthillFinder(){
    double max_val = 0;
    int i;
    int j;
    printf("Hello");
    tvals maxinfo[18];
    int loc1;
    int loc2;
    
    #pragma omp parallel shared(maxinfo)
    {
        int id = omp_get_thread_num();
        printf("id: %d", id);
        maxinfo[id].val = -1.0e30;
        #pragma omp for collapse(2)
        for (i = 7; i < this->m; i+=15){
                // #pragma omp parallel for
                for (j = 7; j < this->m; j+=15){
                    double val = this->number_of_ants_in_cell(i, j);
                    // #pragma omp critical
                    if (val > maxinfo[id].val){
                        maxinfo[id].val = val;
                        maxinfo[id].loc1 = i;
                        maxinfo[id].loc2 = j;
                    }
                }
            }
    }
    #pragma omp flush(maxinfo)
    #pragma omp master
    {
        int nt = omp_get_num_threads();
        // printf("%d\n",nt);
        loc1 = maxinfo[0].loc1;
        loc2 = maxinfo[0].loc2;
        max_val = maxinfo[0].val;
        for (int i = 1; i < 16; ++i){
            if (maxinfo[i].val > max_val){
                max_val = maxinfo[i].val;
                loc1 = maxinfo[i].loc1;
                loc2 = maxinfo[i].loc2;
            }
        }
    }
    int loc3 = 0;
    int loc4 = 0;
    max_val = 0;
    int upperi = loc1 + 13;
    int upperj = loc2 + 13;
    if (loc1 + 15 >= this->m){
        upperi = this->m;
    }
    if (loc2 + 15 >= this->m){
        upperj = this->m;
    }
    // #pragma omp parallel for
    for (int i = max(loc1-10, 0); i < upperi; i+=5){
        // #pragma omp parallel for
        for (int j = max(loc2-10,0); j < upperj; j+=5){
            double val = this->number_of_ants_in_cell(i, j);
            // #pragma omp critical
            if (val > max_val){
                max_val = val;
                loc3 = i;
                loc4 = j;
            }
        }
    }
    upperi = loc3 + 8;
    upperj = loc4 + 8;
    if (loc3 + 5 >= this->m){
        upperi = this->m;
    }
    if (loc4 + 5 >= this->m){
        upperj = this->m;
    }
    int finalLoc1 = 0;
    int finalLoc2 = 0;
    max_val = 0;

    for (int i = max(loc3-7,0); i < upperi && i < this->m; ++i){
        for (int j = max(loc4-7,0); j < upperj && j < this->m; ++j){
            double val = this->number_of_ants_in_cell(i, j);
            if (val > max_val){
                max_val = val;
                finalLoc1 = i;
                finalLoc2 = j;
            }
        }
    }
    
    printf("%d %d %d %d\n", this->anthill_x, this->anthill_y, finalLoc1, finalLoc2);
    assert(finalLoc2==this->anthill_y);
    assert(finalLoc1==this->anthill_x);
}

int main (int argc, char **argv) {

    double start_time, execution_time; // Variables to determine execution time

    Lawn_Class MyLawn;	// Create object MyLawn

    if (argc != 5) { 
	printf("Use: <executable_name> <lawn_size> <anthill_x> <anthill_y> <steps>\n"); 
	printf(" <lawn_size> = number of cells along each dimension of the lawn\n"); 
	printf(" <anthill_x> <anthill_y> = location of the anthill\n"); 
	printf(" <steps> = number of steps the lawn is updated\n"); 
	return 0; 
    }
    int size = atoi(argv[1]); 
    int anthill_x = atoi(argv[2]); 
    int anthill_y = atoi(argv[3]); 
    int steps = atoi(argv[4]);

    // #pragma omp parallel for collapse(2)
    for (int i = 64; i < size; ++i){
        for ( int j = 0; j < size; ++j){
            Lawn_Class MyLawn2;
            MyLawn2.initialize_Lawn(size, i, j, steps);
            printf("init\n");
            MyLawn2.anthillFinder();
        }
    }
    MyLawn.initialize_Lawn(size, anthill_x, anthill_y, steps); 

    MyLawn.save_Lawn_to_file(); // Not recommended when size is large

    // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    // Example Approach #1: Brute force approach

    start_time = omp_get_wtime(); 
    volatile int found = 0;
    // printf("Initialized");
    // #pragma omp parallel for default(none) shared(MyLawn, found)
    // for (int i = 0; i < MyLawn.m; i++) {
    //     for (int j = 0; j < MyLawn.m; j++) {
    //         if (found == 0) {
    //             if (MyLawn.guess_anthill_location(i,j) == 1) {
    //             found = 1;
    //             #pragma omp flush(found)
    //             }
    //         }
    //     }       
    // }

    // int i = int(MyLawn.m/2);
    // int j = int(MyLawn.m/2);
    // int l = MyLawn.m;
    // int m = 0;
    // int a = 0;
    // int b = 0;
    // for (int i = 0; i< MyLawn.m; ++i){
    //     int imax = 0;
    //     int d = 0;
    //     for (int j = 0; j < MyLawn.m; ++j){
    //         int v = MyLawn.number_of_ants_in_cell(i, j);
    //         if (imax < v){
    //             d = j;
    //             imax = v;
    //         }
    //         // imax = max({imax, MyLawn.number_of_ants_in_cell(i,j)})
    //     }
    //     if (m>imax){
    //         break;
    //     }
    //     else{
    //         m = imax;
    //         a = i;
    //         b = d;
    //     }
    // }
    // printf("%d %d\n", a,b);

    // for(int i = 0; i < MyLawn.m; ++i){
    //     int idx;
    //     int max_val;

    //     #pragma omp parallel for reduction(max: max_val)
    //     for (idx = 0; idx < MyLawn.m; ++i){

    //     }
    // }
    
    // int i = 0;
    // int j = 0;
    // // int count = 127;
    // while (i < MyLawn.m-1 && j < MyLawn.m)
    // {
    //     printf("%d %d", i, j);
    //     // count--;
    //     int val = MyLawn.number_of_ants_in_cell(i,j);
    //     if (MyLawn.guess_anthill_location(i,j)==1){
    //         found = 1;
    //         break;
    //     }
    //     else if (MyLawn.number_of_ants_in_cell(i+1,j)>val)
    //     {
    //         ++i;
    //     }
    //     else{
    //         ++j;
    //     }
        
    // }
    // int max_val = 0;
    // int loc1 = 0;
    // int loc2 = 0;
    // int k, row, col, index;
    // #pragma omp parallel for
    // for (k = 0; k < MyLawn.m * MyLawn.m; ++k){
    //     int i = k/MyLawn.m;
    //     int j = k%MyLawn.m;
    //     int val = MyLawn.number_of_ants_in_cell(i, j);
    //     if (val > max_val){
    //         #pragma omp critical
    //         max_val = val;
    //         loc1 = i;
    //         loc2 = j;
    //     }
    // }
    // printf("%d %d", loc1, loc2);
    // double max_val = 0;
    // int i;
    // int j;
    // typedef struct
    // {
    //     double val;
    //     int loc1;
    //     int loc2;
    //     char pad[128];
    // } tvals;
    // tvals maxinfo[18];
    // int loc1;
    // int loc2;
    // #pragma omp parallel shared(maxinfo)
    // {
    //     int id = omp_get_thread_num();
    //     // printf("id: %d", id);
    //     maxinfo[id].val = -1.0e30;
    //     #pragma omp for collapse(2)
    //     for (i = 7; i < MyLawn.m; i+=15){
    //             // #pragma omp parallel for
    //             for (j = 7; j < MyLawn.m; j+=15){
    //                 double val = MyLawn.number_of_ants_in_cell(i, j);
    //                 // #pragma omp critical
    //                 if (val > maxinfo[id].val){
    //                     maxinfo[id].val = val;
    //                     maxinfo[id].loc1 = i;
    //                     maxinfo[id].loc2 = j;
    //                 }
    //             }
    //         }
    // }
    // #pragma omp flush(maxinfo)
    // #pragma omp master
    // {
    //     int nt = omp_get_num_threads();
    //     printf("%d\n",nt);
    //     loc1 = maxinfo[0].loc1;
    //     loc2 = maxinfo[0].loc2;
    //     max_val = maxinfo[0].val;
    //     for (int i = 1; i < 16; ++i){
    //         if (maxinfo[i].val > max_val){
    //             max_val = maxinfo[i].val;
    //             loc1 = maxinfo[i].loc1;
    //             loc2 = maxinfo[i].loc2;
    //         }
    //     }
    // }
    // // max_val = 0;
    // // printf("%d %d\n", loc11, loc22);
    // // #pragma omp parallel for collapse(2) private(i,j) shared(max_val, loc1, loc2)
    // // for (i = 7; i < MyLawn.m; i+=15){
    // //     // #pragma omp parallel for
    // //     for (j = 7; j < MyLawn.m; j+=15){
    // //         double val = MyLawn.number_of_ants_in_cell(i, j);
    // //         // #pragma omp critical
    // //         if (val > max_val){
    // //             max_val = val;
    // //             loc1 = i;
    // //             loc2 = j;
    // //         }
    // //     }
    // // }
    // // double exec_time2 = omp_get_wtime();
    // // printf("%d %d %.8f\n", loc1, loc2,exec_time2-exec_time);
    // int loc3 = 0;
    // int loc4 = 0;
    // max_val = 0;
    // int upperi = loc1+8;
    // int upperj = loc2 + 8;
    // if (loc1 + 15 >= MyLawn.m){
    //     upperi = MyLawn.m;
    // }
    // if (loc2 + 15 >= MyLawn.m){
    //     upperj = MyLawn.m;
    // }
    // // #pragma omp parallel for
    // for (int i = loc1-7; i < upperi; i+=3){
    //     // #pragma omp parallel for
    //     for (int j = loc2-7; j < upperj; j+=3){
    //         double val = MyLawn.number_of_ants_in_cell(i, j);
    //         // #pragma omp critical
    //         if (val > max_val){
    //             max_val = val;
    //             loc3 = i;
    //             loc4 = j;
    //         }
    //     }
    // }
    // printf("%d %d\n", loc3, loc4);
    // int finalLoc1 = 0;
    // int finalLoc2 = 0;
    // max_val = 0;

    // for (int i = loc3-1; i < loc3+2 && i < MyLawn.m; ++i){
    //     for (int j = loc4-1; j < loc4+2 && j < MyLawn.m; ++j){
    //         double val = MyLawn.number_of_ants_in_cell(i, j);
    //         if (val > max_val){
    //             max_val = val;
    //             finalLoc1 = i;
    //             finalLoc2 = j;
    //         }
    //     }
    // }
    MyLawn.anthillFinder();
    // printf("%d %d", finalLoc1, finalLoc2);
    // #pragma parallel for ends here ...
    execution_time = omp_get_wtime() - start_time; 

    MyLawn.report_results(execution_time); 
    // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    return 0;
}
