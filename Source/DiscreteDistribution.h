#pragma once

#include <vector>

// Discrete probability distribution sampling based on alias method
// http://www.keithschwarz.com/darts-dice-coins
template <typename T> struct DiscreteDistribution
{
	typedef std::pair<T, size_t> Cell;

	DiscreteDistribution(const T* weights, size_t count, T weightSum)
	{
		std::vector<Cell> large;
		std::vector<Cell> small;
		for (size_t i = 0; i < count; ++i)
		{
			T p = weights[i] * count / weightSum;
			if (p < T(1))
				small.push_back({p, i});
			else
				large.push_back({p, i});
		}

		m_cells.resize(count, {T(0), 0});

		while (large.size() && small.size())
		{
			auto l = small.back();
			small.pop_back();
			auto g = large.back();
			large.pop_back();
			m_cells[l.second].first  = l.first;
			m_cells[l.second].second = g.second;
			g.first                  = (l.first + g.first) - T(1);
			if (g.first < T(1))
			{
				small.push_back(g);
			}
			else
			{
				large.push_back(g);
			}
		}

		while (large.size())
		{
			auto g = large.back();
			large.pop_back();
			m_cells[g.second].first = T(1);
		}

		while (small.size())
		{
			auto l = small.back();
			small.pop_back();
			m_cells[l.second].first = T(1);
		}
	}

	size_t operator()(u32 u1, float u2) const
	{
		size_t i = u1 % m_cells.size();
		if (u2 <= m_cells[i].first)
		{
			return i;
		}
		else
		{
			return m_cells[i].second;
		}
	}

	std::vector<Cell> m_cells;
};
