#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include <chrono>
#include <vector>
#ifdef _OPENMP
#include <omp.h>
#endif

#define MAX_LOAD_THREADS 4

int number_bacteria;
char** bacteria_name;
long M, M1, M2;
short code[27] = { 0, 2, 1, 2, 3, 4, 5, 6, 7, -1, 8, 9, 10, 11, -1, 12, 13, 14, 15, 16, 1, 17, 18, 5, 19, 3};
#define encode(ch) code[ch-'A']
#define LEN 6
#define AA_NUMBER 20
#define EPSILON 1e-010

void Init()
{
	M2 = 1;
	for (int i=0; i<LEN-2; i++) M2 *= AA_NUMBER;
	M1 = M2 * AA_NUMBER;
	M = M1 * AA_NUMBER;
}

class Bacteria
{
public:
	long* vector;
	long* second;
	long one_l[AA_NUMBER];
	long indexs;
	long total, total_l, complement;
	long count;
	double* tv;
	long *ti;

	void InitVectors()
	{
		vector = new long [M];
		second = new long [M1];
		memset(vector, 0, M * sizeof(long));
		memset(second, 0, M1 * sizeof(long));
		memset(one_l, 0, AA_NUMBER * sizeof(long));
		total = 0; total_l = 0; complement = 0;
	}

	void init_buffer(char* buffer)
	{
		complement++;
		indexs = 0;
		for (int i=0; i<LEN-1; i++) {
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

	Bacteria(char* filename)
	{
		FILE* f;
		errno_t OK = fopen_s(&f, filename, "r");
		if (OK != 0) {
			fprintf(stderr, "Error: failed to open file %s\n", filename);
			exit(1);
		}

		InitVectors();

		char ch;
		while ((ch = fgetc(f)) != EOF)
		{
			if (ch == '>') {
				while (fgetc(f) != '\n');
				char buffer[LEN-1];
				fread(buffer, sizeof(char), LEN-1, f);
				init_buffer(buffer);
			}
			else if (ch != '\n') cont_buffer(ch);
		}

		long tpc = total + complement;
		double half = total * 0.5;
		int a = 0, b = 0;
		long c = 0, d = 0;

		double one_div[AA_NUMBER];
		for (int i=0; i<AA_NUMBER; i++) one_div[i] = (double)one_l[i] / total_l;

		double* sd = new double[M1];
		for (int i=0; i<M1; i++) sd[i] = (double)second[i] / tpc;

		count = 0;
		double* t = new double[M];

		for (long i=0; i<M; i++)
		{
			double p1 = sd[b];
			double p2 = one_div[a];
			double p3 = sd[c];
			double p4 = one_div[d];
			double stoch = (p1 * p2 + p3 * p4) * half;

			if (a == AA_NUMBER-1) { a = 0; b++; }
			else a++;

			if (c == M1-1) { c = 0; d++; }
			else c++;

			if (stoch > EPSILON) {
				t[i] = (vector[i] - stoch) / stoch;
				if (t[i] != 0) count++;
			}
			else t[i] = 0;
		}

		delete[] sd;
		delete[] vector;
		delete[] second;

		tv = new double[count];
		ti = new long[count];

		int pos = 0;
		for (long i=0; i<M; i++)
			if (t[i] != 0) { tv[pos] = t[i]; ti[pos] = i; pos++; }
		delete[] t;

		fclose(f);
	}

	~Bacteria() { delete[] tv; delete[] ti; }
};

void ReadInputFile(const char* input_name)
{
	FILE* f;
	errno_t OK = fopen_s(&f, input_name, "r");
	if (OK != 0) {
		fprintf(stderr, "Error: failed to open file %s (Hint: check your working directory)\n", input_name);
		exit(1);
	}

	fscanf_s(f, "%d", &number_bacteria);
	bacteria_name = new char*[number_bacteria];

	for (long i=0; i<number_bacteria; i++) {
		char name[10];
		fscanf_s(f, "%s", name, 10);
		bacteria_name[i] = new char[20];
		sprintf_s(bacteria_name[i], 20, "data/%s.faa", name);
	}
	fclose(f);
}

double CompareBacteria(Bacteria* b1, Bacteria* b2)
{
	double corr = 0, len1 = 0, len2 = 0;
	long p1 = 0, p2 = 0;
	while (p1 < b1->count && p2 < b2->count)
	{
		long n1 = b1->ti[p1];
		long n2 = b2->ti[p2];
		if (n1 < n2) {
			double t1 = b1->tv[p1];
			len1 += (t1 * t1);
			p1++;
		}
		else if (n2 < n1) {
			double t2 = b2->tv[p2];
			p2++;
			len2 += (t2 * t2);
		}
		else {
			double t1 = b1->tv[p1++];
			double t2 = b2->tv[p2++];
			len1 += (t1 * t1);
			len2 += (t2 * t2);
			corr += t1 * t2;
		}
	}
	while (p1 < b1->count) { double t1 = b1->tv[p1++]; len1 += (t1 * t1); }
	while (p2 < b2->count) { double t2 = b2->tv[p2++]; len2 += (t2 * t2); }

	return corr / (sqrt(len1) * sqrt(len2));
}

void CompareAllBacteria()
{
	using Clock = std::chrono::steady_clock;

	int n = number_bacteria;
	Bacteria** b = new Bacteria*[n];
	std::vector<double> res((size_t)n * n, 0.0);

	int lt = 1;
#ifdef _OPENMP
	lt = omp_get_max_threads();
	if (lt > MAX_LOAD_THREADS) lt = MAX_LOAD_THREADS;
#endif

	auto s1 = Clock::now();

#ifdef _OPENMP
#pragma omp parallel for shared(b, bacteria_name, n) num_threads(lt) schedule(dynamic, 1)
#endif
	for (int i = 0; i < n; i++) b[i] = new Bacteria(bacteria_name[i]);

	auto e1 = Clock::now();

	for (int i = 0; i < n; i++) printf("load %d of %d\n", i + 1, n);

	auto s2 = Clock::now();

	std::vector<int> pi, pj;
	int pc = n * (n - 1) / 2;
	pi.reserve(pc);
	pj.reserve(pc);

	for (int i = 0; i < n - 1; i++)
		for (int j = i + 1; j < n; j++) { pi.push_back(i); pj.push_back(j); }

#ifdef _OPENMP
#pragma omp parallel for shared(b, res, pi, pj, pc, n) schedule(dynamic, 1)
#endif
	for (int k = 0; k < pc; ++k) {
		int i = pi[k];
		int j = pj[k];
		res[(size_t)i * n + j] = CompareBacteria(b[i], b[j]);
	}

	auto e2 = Clock::now();

	for (int i = 0; i < n - 1; i++)
		for (int j = i + 1; j < n; j++)
			printf("%2d %2d -> %.20lf\n", i, j, res[(size_t)i * n + j]);

	printf("vector preparation: %.6f seconds\n", std::chrono::duration<double>(e1 - s1).count());
	printf("pair comparisons: %.6f seconds\n", std::chrono::duration<double>(e2 - s2).count());

	for (int i = 0; i < n; i++) delete b[i];
	delete[] b;
}

int main(int argc,char * argv[])
{
	using Clock = std::chrono::steady_clock;
	auto start = Clock::now();

	Init();
	ReadInputFile("list.txt");
	CompareAllBacteria();

	for (int i = 0; i < number_bacteria; i++) delete[] bacteria_name[i];
	delete[] bacteria_name;

	auto end = Clock::now();
	printf("time elapsed: %.6f seconds\n", std::chrono::duration<double>(end - start).count());
	return 0;
}
