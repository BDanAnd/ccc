#ifndef BITVECTOR_H
#define BITVECTOR_H

#include <iostream>
#include <vector>
#include <functional>
#include <iterator>

using namespace std;

class bitvector
{

private:
    vector<bool> bits;

public:
    bitvector(unsigned a = 0, bool b = false) { bits.assign(a, b); }

    bitvector(const vector<bool>& a) { bits = a; }

    bitvector& operator=(const bitvector& other);

    static const bitvector op(const bitvector& left, const bitvector& right, function<bool(bool, bool)> fn);

    vector<bool>::reference operator[](int i);

    vector<bool>::const_reference operator[](int i) const;

    friend const bitvector operator+(const bitvector& left, const bitvector& right);

    friend const bitvector operator-(const bitvector& left, const bitvector& right);

    friend const bitvector operator*(const bitvector& left, const bitvector& right);

    friend bool operator==(const bitvector& left, const bitvector& right);

    friend ostream& operator<<(ostream& os, const bitvector& d);

    operator vector<int>();
};

#endif
