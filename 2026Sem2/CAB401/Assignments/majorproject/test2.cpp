// =====================================================================
// improved_parallel.cpp
// OpenMP-parallelised version of improvedbefore.cpp
//
// Every change from improvedbefore.cpp is tagged with [OMP-n] so it can
// be found with a text search. Summary:
//   [OMP-1] new headers (<chrono>, <vector>, <omp.h>) and MAX_LOAD_THREADS
//   [OMP-2] count bug fix in Bacteria constructor (only count non-zero t[i])
//   [OMP-3] delete -> delete[] for all arrays
//   [OMP-4] Bacteria destructor (frees tv / ti)
//   [OMP-5] CompareAllBacteria: parallel loading + parallel pair comparison
//   [OMP-6] main: chrono timing + cleanup of bacteria_name
//
// Build (MSVC):  cl /O2 /openmp /EHsc improved_parallel.cpp
// Build (GCC)  :  g++ -O2 -fopenmp improved_parallel.cpp   (needs the MSVC
//                 *_s functions, so MSVC is the intended compiler)
// =====================================================================
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include <chrono>      // [OMP-1] wall-clock timing of each stage
#include <vector>      // [OMP-1] results matrix and pair list
#ifdef _OPENMP
#include <omp.h>       // [OMP-1] omp_get_max_threads()
#endif
 
// [OMP-1] Each Bacteria being loaded temporarily needs several arrays of
// size M = 20^6 (vector, t) plus M1 (second, second_div_total). That is
// roughly 0.7 - 1.5 GB per loading thread depending on sizeof(long), so
// the loading stage is capped to keep RAM use under control. Raise this
// if the machine has plenty of memory; the comparison stage is NOT
// capped because it only reads the (small) sparse vectors.
#define MAX_LOAD_THREADS	4
 
int number_bacteria;
char** bacteria_name;
long M, M1, M2;
short code[27] = { 0, 2, 1, 2, 3, 4, 5, 6, 7, -1, 8, 9, 10, 11, -1, 12, 13, 14, 15, 16, 1, 17, 18, 5, 19, 3};
#define encode(ch)		code[ch-'A']
#define LEN				6
#define AA_NUMBER		20
#define	EPSILON			1e-010
 
void Init()
{
	M2 = 1;
	for (int i=0; i<LEN-2; i++)	// M2 = AA_NUMBER ^ (LEN-2);
		M2 *= AA_NUMBER; 
	M1 = M2 * AA_NUMBER;		// M1 = AA_NUMBER ^ (LEN-1);
	M  = M1 *AA_NUMBER;			// M  = AA_NUMBER ^ (LEN);
}
 
class Bacteria
{
private:
	long* vector;
	long* second;
	long one_l[AA_NUMBER];
	long indexs;
	long total;
	long total_l;
	long complement;
 
	void InitVectors()
	{
		vector = new long [M];
		second = new long [M1];
		memset(vector, 0, M * sizeof(long));
		memset(second, 0, M1 * sizeof(long));
		memset(one_l, 0, AA_NUMBER * sizeof(long));
		total = 0;
		total_l = 0;
		complement = 0;
	}
 
	void init_buffer(char* buffer)
	{
		complement++;
		indexs = 0;
		for (int i=0; i<LEN-1; i++)
		{
			short enc = encode(buffer[i]);
			one_l[enc]++;
			total_l++;
			indexs = indexs * AA_NUMBER + enc;
		}
		second[indexs]++;
	}
 
	void cont_buffer(char ch)
	{
		short enc = encode(ch);
		one_l[enc]++;
		total_l++;
		long index = indexs * AA_NUMBER + enc;
		vector[index]++;
		total++;
		indexs = (indexs % M2) * AA_NUMBER + enc;
		second[indexs]++;
	}
 
public:
	long count;
	double* tv;
	long *ti;
 
	// NOTE: the constructor only touches this object's own members (plus
	// read-only globals M, M1, M2, code), so several Bacteria objects can
	// safely be constructed at the same time from different threads.
	Bacteria(char* filename)
	{
		FILE* bacteria_file;
		errno_t OK = fopen_s(&bacteria_file, filename, "r");
 
		if (OK != 0)
		{
			fprintf(stderr, "Error: failed to open file %s\n", filename);
			exit(1);
		}
 
		InitVectors();
 
		char ch;
		while ((ch = fgetc(bacteria_file)) != EOF)
		{
			if (ch == '>')
			{
				while (fgetc(bacteria_file) != '\n'); // skip rest of line
 
				char buffer[LEN-1];
				fread(buffer, sizeof(char), LEN-1, bacteria_file);
				init_buffer(buffer);
			}
			else if (ch != '\n')
				cont_buffer(ch);
		}
 
		long total_plus_complement = total + complement;
		double total_div_2 = total * 0.5;
		int i_mod_aa_number = 0;
		int i_div_aa_number = 0;
		long i_mod_M1 = 0;
		long i_div_M1 = 0;
 
		double one_l_div_total[AA_NUMBER];
		for (int i=0; i<AA_NUMBER; i++)
			one_l_div_total[i] = (double)one_l[i] / total_l;
		
		double* second_div_total = new double[M1];
		for (int i=0; i<M1; i++)
			second_div_total[i] = (double)second[i] / total_plus_complement;
 
		count = 0;
		double* t = new double[M];
 
		for(long i=0; i<M; i++)
		{
			double p1 = second_div_total[i_div_aa_number];
			double p2 = one_l_div_total[i_mod_aa_number];
			double p3 = second_div_total[i_mod_M1];
			double p4 = one_l_div_total[i_div_M1];
			double stochastic =  (p1 * p2 + p3 * p4) * total_div_2;
 
			if (i_mod_aa_number == AA_NUMBER-1)
			{
				i_mod_aa_number = 0;
				i_div_aa_number++;
			}
			else
				i_mod_aa_number++;
 
			if (i_mod_M1 == M1-1)
			{
				i_mod_M1 = 0;
				i_div_M1++;
			}
			else
				i_mod_M1++;
 
			if (stochastic > EPSILON) 
			{
				t[i] = (vector[i] - stochastic) / stochastic;
 
				// [OMP-2] Only count entries that will actually be stored.
				// The original counted every entry with stochastic > EPSILON,
				// even when t[i] == 0, so count could exceed the number of
				// values copied below and leave uninitialised tv/ti slots,
				// which would corrupt the merge in CompareBacteria().
				if (t[i] != 0)
					count++;
			}
			else
				t[i] = 0;
		}
		
		delete[] second_div_total;	// [OMP-3] was: delete second_div_total;
		delete[] vector;			// [OMP-3] was: delete vector;
		delete[] second;			// [OMP-3] was: delete second;
 
		tv = new double[count];
		ti = new long[count];
 
		int pos = 0;
		for (long i=0; i<M; i++)
		{
			if (t[i] != 0)
			{
				tv[pos] = t[i];
				ti[pos] = i;
				pos++;
			}
		}
		delete[] t;					// [OMP-3] was: delete t;
 
		fclose (bacteria_file);
	}
 
	// [OMP-4] Release the sparse vectors when a Bacteria is deleted.
	~Bacteria()
	{
		delete[] tv;
		delete[] ti;
	}
};
 
void ReadInputFile(const char* input_name)
{
	FILE* input_file;
	errno_t OK = fopen_s(&input_file, input_name, "r");
 
	if (OK != 0)
	{
		fprintf(stderr, "Error: failed to open file %s (Hint: check your working directory)\n", input_name);
		exit(1);
	}
 
	fscanf_s(input_file, "%d", &number_bacteria);
	bacteria_name = new char*[number_bacteria];
 
	for(long i=0;i<number_bacteria;i++)
	{
		char name[10];
		fscanf_s(input_file, "%s", name, 10);
		bacteria_name[i] = new char[20];
		sprintf_s(bacteria_name[i], 20, "data/%s.faa", name);
	}
	fclose(input_file);
}
 
// Unchanged. It only READS b1 and b2 and uses local accumulators, so any
// number of pairs can be compared concurrently without locking.
double CompareBacteria(Bacteria* b1, Bacteria* b2)
{
	double correlation = 0;
	double vector_len1=0;
	double vector_len2=0;
	long p1 = 0;
	long p2 = 0;
	while (p1 < b1->count && p2 < b2->count)
	{
		long n1 = b1->ti[p1];
		long n2 = b2->ti[p2];
		if (n1 < n2)
		{
			double t1 = b1->tv[p1];
			vector_len1 += (t1 * t1);
			p1++;
		}
		else if (n2 < n1)
		{
			double t2 = b2->tv[p2];
			p2++;
			vector_len2 += (t2 * t2);
		}
		else
		{
			double t1 = b1->tv[p1++];
			double t2 = b2->tv[p2++];
			vector_len1 += (t1 * t1);
			vector_len2 += (t2 * t2);
			correlation += t1 * t2;
		}
	}
	while (p1 < b1->count)
	{
		long n1 = b1->ti[p1];
		double t1 = b1->tv[p1++];
		vector_len1 += (t1 * t1);
	}
	while (p2 < b2->count)
	{
		long n2 = b2->ti[p2];
		double t2 = b2->tv[p2++];
		vector_len2 += (t2 * t2);
	}
 
	return correlation / (sqrt(vector_len1) * sqrt(vector_len2));
}
 
// [OMP-5] Rewritten. Two parallel stages with a fixed-order print at the end.
void CompareAllBacteria()
{
	using Clock = std::chrono::steady_clock;
 
	// Deliberately non-const: older compilers reject const variables in a
	// shared() clause, newer ones require them to be listed (default(none)).
	int n = number_bacteria;
	Bacteria** b = new Bacteria*[n];
 
	// [OMP-5a] Results are stored, not printed from inside the parallel loop,
	// so output order is deterministic and identical to the serial program.
	// Each pair (i,j) writes to its own slot, so there is no data race.
	std::vector<double> results((size_t)n * n, 0.0);
 
	// [OMP-5b] Stage 1: load/build the n composition vectors in parallel.
	// Each iteration is independent (one file -> one Bacteria). Loading
	// time per file varies, so schedule(dynamic,1) hands out one file at a
	// time. Thread count is capped by MAX_LOAD_THREADS to bound memory.
	int load_threads = 1;
#ifdef _OPENMP
	load_threads = omp_get_max_threads();
	if (load_threads > MAX_LOAD_THREADS)
		load_threads = MAX_LOAD_THREADS;
#endif
 
	const auto preparation_start = Clock::now();
 
	#ifdef _OPENMP
	#pragma omp parallel for default(none) shared(b, bacteria_name, n) num_threads(load_threads) schedule(dynamic, 1)
	#endif
	for (int i = 0; i < n; i++)
	{
		b[i] = new Bacteria(bacteria_name[i]);
	}
 
	const auto preparation_end = Clock::now();
 
	// Progress lines are printed after loading (serial) so they never interleave.
	for (int i = 0; i < n; i++)
		printf("load %d of %d\n", i + 1, n);
 
	const auto comparison_start = Clock::now();
 
	// [OMP-5c] Stage 2: flatten the triangular (i<j) loop into a list of
	// pairs. A triangular nested loop gives uneven work if split by i
	// (row 0 has n-1 pairs, the last row has 1); one flat loop over pairs
	// with dynamic scheduling balances well. Setup cost is included in the
	// comparison timing.
	std::vector<int> pair_i;
	std::vector<int> pair_j;
 
	int pair_count = n * (n - 1) / 2;
	pair_i.reserve(pair_count);
	pair_j.reserve(pair_count);
 
	for (int i = 0; i < n - 1; i++)
		for (int j = i + 1; j < n; j++)
		{
			pair_i.push_back(i);
			pair_j.push_back(j);
		}
 
	// CompareBacteria only reads b[i], b[j]; the only write is to results[i*n+j].
	#ifdef _OPENMP
	#pragma omp parallel for default(none) shared(b, results, pair_i, pair_j, pair_count, n) schedule(dynamic, 1)
	#endif
	for (int k = 0; k < pair_count; ++k)
	{
		const int i = pair_i[k];
		const int j = pair_j[k];
		results[(size_t)i * n + j] = CompareBacteria(b[i], b[j]);
	}
 
	const auto comparison_end = Clock::now();
 
	for (int i = 0; i < n - 1; i++)
		for (int j = i + 1; j < n; j++)
			printf("%2d %2d -> %.20lf\n", i, j, results[(size_t)i * n + j]);
 
	printf("vector preparation: %.6f seconds\n",
	       std::chrono::duration<double>(preparation_end - preparation_start).count());
	printf("pair comparisons: %.6f seconds\n",
	       std::chrono::duration<double>(comparison_end - comparison_start).count());
 
		delete b[i];
	delete[] b;
}
 
int main(int argc,char * argv[])
{
	using Clock = std::chrono::steady_clock;
	const auto start = Clock::now();
 
	Init();
	ReadInputFile("list.txt");
	CompareAllBacteria();
 
	for (int i = 0; i < number_bacteria; i++)
		delete[] bacteria_name[i];
	delete[] bacteria_name;
 
	const auto end = Clock::now();
	printf("time elapsed: %.6f seconds\n",
	       std::chrono::duration<double>(end - start).count());
	return 0;
}