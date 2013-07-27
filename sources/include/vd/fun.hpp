/** VD */
#pragma once

#include <functional>
#include <vector>
#include <list>

namespace vd {

// Sometimes I'm fan of fun
namespace fun {

template <typename T, typename F>
void each(const T& cnt, const F& functor)
{
	std::for_each(cnt.begin(), cnt.end(), functor);
}

template <typename Orig, typename New>
std::vector<New> transform(const std::vector<Orig>& cnt, const std::function<New (const Orig& i)>& transf)
{
	std::vector<New> res;
	res.reserve(cnt.size());
	each(cnt, [&] (const Orig& v) {
		res.push_back(transf(v));
	});
	return res;
}

template <typename Orig, typename New>
std::list<New> transform(const std::list<Orig>& cnt, const std::function<New (const Orig& i)>& transf)
{
	std::list<New> res;
	each(cnt, [&] (const Orig& v) {
		res.push_back(transf(v));
	});
	return res;
}

template <typename T, typename P, typename Cmp>
P best(const P& def, const std::vector<T>& cnt, Cmp cmp, const std::function<P (const T& i)>& prop)
{
	P bst = def;
	each(cnt, [&] (const T& v) 
	{
		P cur = prop(v);
		if (cmp(cur, bst))
			bst = cur;
	});
	return bst;
}

template <typename T>
const T& find(const std::vector<T>& cnt, const T& def, const std::function<bool (const T& i)>& functor)
{
	std::vector<T>::const_iterator i   = cnt.begin();
	std::vector<T>::const_iterator end = cnt.end();

	for (; i != end; ++i)
	{
		if (functor(*i))
			return *i;
	}
	return def;
}

} // namespace fun

} // namespace vd