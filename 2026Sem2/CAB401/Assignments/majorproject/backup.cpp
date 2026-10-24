#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include <chrono>
#include <vector>

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

				if (t[i] != 0) //onyl stores nonzero values
					count++;
			}
			else
				t[i] = 0;
		}
		
		delete[] second_div_total;
		delete[] vector;
		delete[] second;

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
		delete[] t;

		fclose (bacteria_file);
	}

	 ~Bacteria() //release sparse vectors when a Bacteria object is deleted
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

void CompareAllBacteria()
{
	using Clock = std::chrono::steady_clock;

    int n = number_bacteria;
    Bacteria** b = new Bacteria*[n];

    
    std::vector<std::vector<double>> results(
        n, std::vector<double>(n, 0.0)); // Only the entries with j > i will be used

    const auto preparation_start = Clock::now();

    // Stage 1: prepare each dataset's composition vector
	#ifdef _OPENMP
	#pragma omp parallel for default(none) shared(b, bacteria_name, n) schedule(dynamic, 1)
	#endif
    for (int i = 0; i < n; i++)
    {
        b[i] = new Bacteria(bacteria_name[i]);
    }

    const auto preparation_end = Clock::now();

    for (int i = 0; i < n; i++)
        printf("load %d of %d\n", i + 1, n);

    const auto comparison_start = Clock::now();

    // Build a list of every unique pair.
	// Include this setup cost in the comparison-stage timing.
	std::vector<int> pair_i;
	std::vector<int> pair_j;

	const int pair_count = n * (n - 1) / 2;

	pair_i.reserve(pair_count);
	pair_j.reserve(pair_count);

    for (int i = 0; i < n - 1; i++)
    {
        for (int j = i + 1; j < n; j++)
        {
            pair_i.push_back(i);
        	pair_j.push_back(j);
        }
    }

	// Distribute individual pairs among threads.
	#ifdef _OPENMP
	#pragma omp parallel for default(none) shared(b, results, pair_i, pair_j, pair_count) schedule(dynamic, 1)
	#endif
	for (int k = 0; k < pair_count; ++k)
	{
		const int i = pair_i[k];
		const int j = pair_j[k];

		results[i][j] = CompareBacteria(b[i], b[j]);
	}

    const auto comparison_end = Clock::now();

    // Print in a fixed order after all calculations finish
    for (int i = 0; i < n - 1; i++)
    {
        for (int j = i + 1; j < n; j++)
        {
            printf("%2d %2d -> %.20lf\n", i, j, results[i][j]);
        }
    }

    const double preparation_seconds =
        std::chrono::duration<double>(
            preparation_end - preparation_start).count();

    const double comparison_seconds =
        std::chrono::duration<double>(
            comparison_end - comparison_start).count();

    printf("vector preparation: %.6f seconds\n",
           preparation_seconds);

    printf("pair comparisons: %.6f seconds\n",
           comparison_seconds);


	for (int i = 0; i < n; i++)
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

	for (int i = 0; i < number_bacteria; i++) //delete each filename string
    	delete[] bacteria_name[i];
	delete[] bacteria_name; //delete the array of filename pointers

	const auto end = Clock::now();

    const double elapsed =
        std::chrono::duration<double>(end - start).count();

	printf("time elapsed: %.6f seconds\n", elapsed); 
	return 0;
}