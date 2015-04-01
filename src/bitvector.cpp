#include "bitvector.h"

bitvector& bitvector::operator=(const bitvector& other)
{
	if (this != &other)
		bits = other.bits;
	return *this;
}

const bitvector bitvector::op(const bitvector& left, const bitvector& right, function<bool(bool, bool)> fn)
{
	vector<bool> tmp;
	vector<bool>::const_iterator a = left.bits.begin(), b = right.bits.begin();
	while (a != left.bits.end() && b != right.bits.end()) {
		tmp.push_back(fn(*a, *b));
		++a;
		++b;
	}
	return bitvector(tmp);
}

vector<bool>::reference bitvector::operator[](int i) {
	return bits[i];
}

vector<bool>::const_reference bitvector::operator[](int i) const {
	return bits[i];
}

const bitvector operator+(const bitvector& left, const bitvector& right)
{
	return bitvector::op(left, right, [](bool a, bool b){return a | b;});
}

const bitvector operator-(const bitvector& left, const bitvector& right)
{
	return bitvector::op(left, right, [](bool a, bool b){return a & ~b;});
}

const bitvector operator*(const bitvector& left, const bitvector& right)
{
	return bitvector::op(left, right, [](bool a, bool b){return a & b;});
}

bool operator==(const bitvector& left, const bitvector& right)
{
	return left.bits == right.bits;
}

ostream& operator<<(ostream& os, const bitvector& d)
{
	//bit mask
	for (auto it = d.bits.begin(), e = d.bits.end(); it != e; ++it)
		os << (int)*it;
	return os;
}
