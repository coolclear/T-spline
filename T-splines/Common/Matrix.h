#ifndef MATRIX_H
#define MATRIX_H

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <cassert>
#include <iostream>

using namespace std;

// F9
#ifndef DELETE_OPS
#define DELETE_OPS
#define CLEAN_DELETE(p) if(p) delete p
#define CLEAN_ARRAY_DELETE(p) if(p) delete [] p
#endif

template <class Type, int N>
class Matrix
{
	template <class Type, int N> class Vector;
	friend class Vector<Type, N>;

protected:
	Type **data;

	int pivot(int row) {
		int k = row;
		double amax = -1;
		for(int i = row; i < N; i++) {
			double temp = abs(data[i][row]);
			if(temp > 1e-12 && amax < temp) {
				amax = temp;
				k = i;
			}
		}

		if(abs(data[k][row]) < 1e-12)
			return -1;
		if(k != row)
			swap(data[k], data[row]);
		return k;
	}

public:
	Matrix() : data(NULL) {
		data = new Type*[N];
		for(int r = 0; r < N; r++)
			data[r] = new Type[N];
		identity();
	}

	Matrix(const Matrix<Type, N> &A) {
		data = new Type*[N];
		for(int r = 0; r < N; r++) {
			data[r] = new Type[N];
			memcpy(data[r], A.data[r], sizeof(Type) * N);
		}
	}
	virtual ~Matrix() { clean(); }

	Type **Data() { return data; }

	void clean() {
		if(data) {
			for(int r = 0; r < N; r++)
				CLEAN_ARRAY_DELETE(data[r]);
			CLEAN_ARRAY_DELETE(data);
			data = NULL;
		}
	}

	void identity() {
		for(int i = 0; i < N; i++)
			for(int j = 0; j < N; j++)
				data[i][j] = Type(i == j);
	}
	void clear() {
		for(int i = 0; i < N; i++)
			for(int j = 0; j < N; j++)
				data[i][j] = Type(0);
	}
	int size() const { return N; }

	void operator= (const Matrix<Type, N> &A) {
		if(data == NULL) {
			data = new Type*[N];
			for(int r = 0; r < N; r++)
				data[r] = new Type[N];
		}
		for(int r = 0; r < N; r++)
			memcpy(data[r], A.data[r], sizeof(Type) * N);
	}

	inline Type*& operator[] (int r) {
		assert(r >= 0 && r < N);
		return data[r];
	}
	inline Type *operator[] (int r) const {
		assert(r >= 0 && r < N);
		return data[r];
	}


	/************************* FRIEND FUNCTIONS FOR MATRICES *****************************/
	friend Matrix<Type, N> operator+ (const Matrix<Type, N> &L, const Matrix<Type, N> &R) {
		Matrix<Type, N> A;
		for(int r = 0; r < N; r++)
			for(int c = 0; c < N; c++)
				A.data[r][c] = L.data[r][c] + R.data[r][c];
		return move(A);
	}

	friend Matrix<Type, N> operator* (const Matrix<Type, N> &L, const Matrix<Type, N> &R) {
		Matrix<Type, N> A;
		for(int r = 0; r < N; r++) {
			for(int c = 0; c < N; c++) {
				A.data[r][c] = Type(0);
				for(int k = 0; k < N; k++)
					A.data[r][c] += L.data[r][k] * R.data[k][c];
			}
		}
		return move(A);
	}

	friend Matrix<Type, N> operator* (const Matrix<Type, N> &L, Type alpha) { return move(alpha * L); }
	friend Matrix<Type, N> operator* (Type alpha, const Matrix<Type, N> &R) {
		Matrix<Type, N> A;
		for(int r = 0; r < N; r++)
			for(int c = 0; c < N; c++)
				A.data[r][c] = alpha * R.data[r][c];
		return move(A);
	}

	friend Matrix<Type, N> operator- (const Matrix<Type, N> &B) {
		Matrix<Type, N> A;
		for(int r = 0; r < N; r++)
			for(int c = 0; c < N; c++)
				A.data[r][c] = -B.data[r][c];
		return move(A);
	}

	friend Matrix<Type, N> operator- (const Matrix<Type, N> &L, const Matrix<Type, N> &R) {
		Matrix<Type, N> A;
		for(int r = 0; r < N; r++)
			for(int c = 0; c < N; c++)
				A.data[r][c] = L.data[r][c] - R.data[r][c];
		return move(A);
	}

	friend Matrix<Type, N> transpose(const Matrix<Type, N> &B) {
		Matrix<Type, N> A;
		for(int r = 0; r < N; r++)
			for(int c = 0; c < N; c++)
				A.data[c][r] = B.data[r][c];
		return move(A);
	}

	friend Matrix<Type, N> operator! (const Matrix<Type, N> &B) {
		int i, j, k;
		Type a1, a2;
		Matrix<Type, N> A = B, AI;

		for(k = 0; k < N; k++) {
			int indx = A.pivot(k);
			if(indx == -1) {
				cout << "Inversion of a singular matrix" << endl;
				return AI;
			}
			if(indx != k) swap(AI.data[k], AI.data[indx]);

			a1 = A.data[k][k];
			for(j = 0; j < N; j++) {
				A.data[k][j] /= a1;
				AI.data[k][j] /= a1;
			}
			for(i = 0; i < N; i++) if(i != k) {
				a2 = A.data[i][k];
				for(j = 0; j < N; j++) {
					A.data[i][j] -= a2 * A.data[k][j];
					AI.data[i][j] -= a2 * AI.data[k][j];
				}
			}
		}

		return move(AI);
	}
};

template <class Type, int N>
class Vector
{
	friend class Matrix<Type, N>;

protected:
	Type data[N];

public:
	Vector() { zero(); }

	// For N == 2 or 3
	Vector(Type a, Type b) {
		data[0] = a; data[1] = b;
		if(N > 2) data[2] = 1;
	}

	// For N == 3 or 4
	Vector(Type a, Type b, Type c) {
		assert(N >= 3);
		data[0] = a; data[1] = b; data[2] = c;
		if(N > 3) data[3] = 1;
	}

	// For N == 4 only
	Vector(Type a, Type b, Type c, Type d) {
		assert(N == 4);
		data[0] = a; data[1] = b; data[2] = c; data[3] = d;
	}

	Vector(const Vector<Type, N> &vec) { *this = vec; }
	~Vector() {}

	void operator+= (const Vector<Type, N> &vec) {
		for(int i = 0; i < N; i++)
			data[i] += vec.data[i];
	}
	void operator-= (const Vector<Type, N> &vec) {
		for(int i = 0; i < N; i++)
			data[i] -= vec.data[i];
	}
	void operator*= (Type alpha) {
		for(int i = 0; i < N; i++)
			data[i] *= alpha;
	}
	void operator/= (Type alpha) {
		for(int i = 0; i < N; i++)
			data[i] /= alpha;
	}
	void operator= (const Vector<Type, N> &vec) {
		memcpy(data, vec.data, sizeof(Type) * N);
	}

	void print() const { cout << *this; }
	friend ostream& operator<< (ostream &out, const Vector<Type, N> U) {
		for(int i = 0; i < N; i++) {
			if(i) out << ' ';
			out << U.data[i];
		}
		return out;
	}
	void read(istream &in, int n = N) {
		if(n > N) n = N;
		for(int i = 0; i < n; i++)
			in >> data[i];
	}

	inline void zero() { for(int i = 0; i < N; i++) data[i] = Type(0); }
	int size() const { return N; }
	void normalize() {
		Type m = mag(*this);
		if(m > 1e-20) for(int i = 0; i < N; i++) data[i] /= m;
	}

	inline Type& operator[] (int i) { return data[i]; }
	inline Type operator[] (int i) const { return data[i]; }

	friend Vector<Type, N> operator+ (const Vector<Type, N> &U, const Vector<Type, N> &V) {
		Vector<Type, N> W;
		for(int i = 0; i < N; i++)
			W.data[i] = U.data[i] + V.data[i];
		return move(W);
	}
	friend Vector<Type, N> operator- (const Vector<Type, N> &U, const Vector<Type, N> &V) {
		Vector<Type, N> W;
		for(int i = 0; i < N; i++)
			W.data[i] = U.data[i] - V.data[i];
		return move(W);
	}
	friend Vector<Type, N> operator- (const Vector<Type, N> &U) {
		Vector<Type, N> V;
		for(int i = 0; i < N; i++)
			V.data[i] = -U.data[i];
		return move(V);
	}

	friend Vector<Type, N> operator* (const Vector<Type, N> &U, Type alpha) { return move(alpha * U); }
	friend Vector<Type, N> operator* (Type alpha, const Vector<Type, N> &U) {
		Vector<Type, N> V;
		for(int i = 0; i < N; i++)
			V.data[i] = alpha * U.data[i];
		return move(V);
	}

	// row_vec_U * mat_A => row_vec_UA
	friend Vector<Type, N> operator* (const Vector<Type, N> &U, const Matrix<Type, N> &A) {
		Vector<Type, N> UA;
		for(int c = 0; c < N; c++) {
			UA.data[c] = 0;
			for(int r = 0; r < N; r++)
				UA.data[c] += U.data[r] * A[r][c];
		}
		return move(UA);
	}

	// mat_A * col_vec_U => col_vec_AU
	friend Vector<Type, N> operator* (const Matrix<Type, N> &A, const Vector<Type, N> &U) {
		Vector<Type, N> AU;
		for(int r = 0; r < N; r++) {
			AU.data[r] = 0;
			for(int c = 0; c < N; c++)
				AU.data[r] += U.data[c] * A[r][c];
		}
		return move(AU);
	}

	// Dot product between two vectors U, V
	friend Type operator* (const Vector<Type, N> &U, const Vector<Type, N> &V) {
		Type res = 0;
		for(int i = 0; i < N; i++) res += U.data[i] * V.data[i];
		return res;
	}

	inline friend Type mag(const Vector<Type, N> &U) { return sqrt(U*U); }
	// Squared magnitude (avoids sqrt computation)
	inline friend Type mag2(const Vector<Type, N> &U) { return U*U; }

	friend Vector<Type, N> cross(const Vector<Type, N> &U, const Vector<Type, N> &V) {
		assert(N >= 3 && N <= 4);
		Vector<Type, N> res;
		res.data[0] = U.data[1] * V.data[2] - U.data[2] * V.data[1];
		res.data[1] = U.data[2] * V.data[0] - U.data[0] * V.data[2];
		res.data[2] = U.data[0] * V.data[1] - U.data[1] * V.data[0];
		if(N == 4) res.data[3] = 0;
		return move(res);
	}
};

typedef Vector<double, 4> Pt3;
typedef Pt3 Vec3;
typedef Vector<double, 4> Color;
typedef Matrix<double, 4> Mat4;

#endif // Matrix_h