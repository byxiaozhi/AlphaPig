#include "network.cpp"
#include <iostream>
#include <algorithm>
#include <vector>
#include <queue>
#include <limits>
#include <tuple>
#include <cmath>

enum
{
	AI = 1,  // AI������
	OP = -1, // ���ֵ�����
	BL = 0,  // �հ�
};

class AlphaPig
{
public:

	// ���״̬
	int board[81] = { 0 };
	bool air_vis[81];

	// ����
	Network* net;

	// ���ؿ������ڵ�
	struct TreeNode
	{
		// �Ƿ�Ϊ�½ڵ�
		bool is_new = true;

		// ���̼���ɫ
		int board[81] = { 0 };
		int color;

		// ��һ������λ��
		std::vector<int> available_last;
		std::vector<int> available_next;

		// �ڵ���Ӯͳ��
		double value = 0;
		int total = 0;

		// �������
		float net_policy[81];
		float net_value;

		// ���ڵ�
		TreeNode* father = nullptr;

		// ���ӽڵ�
		TreeNode* children[81] = { nullptr };

		// ���һ��λ��
		int last_step = -1;

		// �Ƿ�����
		bool first;
	};

	TreeNode* root = nullptr;

	// �ƶ�λ��
	int moveTo(int p, int dir)
	{
		switch (dir)
		{
		case 0:
			return (p += 9) < 81 ? p : -1;
		case 1:
			return (p -= 9) >= 0 ? p : -1;
		case 2:
			return p % 9 < 8 ? p + 1 : -1;
		case 3:
			return p % 9 > 0 ? p - 1 : -1;
		}
		return p;
	}

	// �ж��Ƿ�����
	bool hasAir(int m_board[], int p)
	{
		air_vis[p] = true;
		bool flag = false;
		for (int dir = 0; dir < 4; dir++)
		{
			int dp = moveTo(p, dir);
			if (dp >= 0)
			{
				if (m_board[dp] == BL)
					flag = true;
				if (m_board[dp] == m_board[p] && !air_vis[dp])
					if (hasAir(m_board, dp))
						flag = true;
			}
		}
		return flag;
	}

	// �ж��Ƿ��������
	bool judgeAvailable(int m_board[], int p, int col)
	{
		if (m_board[p])
			return false;
		m_board[p] = col;
		memset(air_vis, 0, sizeof(air_vis));
		if (!hasAir(m_board, p))
		{
			m_board[p] = 0;
			return false;
		}
		for (int dir = 0; dir < 4; dir++)
		{
			int dp = moveTo(p, dir);
			if (dp >= 0)
			{
				if (m_board[dp] && !air_vis[dp])
					if (!hasAir(m_board, dp))
					{
						m_board[p] = 0;
						return false;
					}
			}
		}
		m_board[p] = 0;
		return true;
	}

	// ɨ��������ӵ�λ��
	void scanAvailable(TreeNode* node)
	{
		for (auto i : node->father->available_last)
			if (judgeAvailable(node->board, i, -node->color))
				node->available_next.push_back(i);

		for (auto i : node->father->available_next)
			if (judgeAvailable(node->board, i, node->color))
				node->available_last.push_back(i);
	}

	// ���ҷ�ת
	void flipData(float* data, int size, int channels)
	{
		for (int k = 0; k < channels; ++k)
			for (int i = 0; i < size; ++i)
				for (int j = 0; j < size / 2; ++j)
				{
					int index = j + size * (i + size * k);
					int flip = (size - j - 1) + size * (i + size * k);
					std::swap(data[index], data[flip]);
				}
	}

	// ��ת
	void rotateData(float* data, int size, int channels, int times)
	{
		times = (times % 4 + 4) % 4;
		for (int i = 0; i < times; ++i)
			for (int c = 0; c < channels; ++c)
				for (int x = 0; x < size / 2; ++x)
					for (int y = 0; y < (size - 1) / 2 + 1; ++y)
					{
						float temp = data[y + size * (x + size * c)];
						data[y + size * (x + size * c)] = data[size - 1 - x + size * (y + size * c)];
						data[size - 1 - x + size * (y + size * c)] = data[size - 1 - y + size * (size - 1 - x + size * c)];
						data[size - 1 - y + size * (size - 1 - x + size * c)] = data[x + size * (size - 1 - y + size * c)];
						data[x + size * (size - 1 - y + size * c)] = temp;
					}
	}

	// ����Ԥ��
	void predict(TreeNode* node)
	{
		// Ԥ�������һ���ڵ㣬��һ�ڵ�Ϊ���Է�������һ�ڵ�Ϊ���ҷ�����������ɫ�����֡������ֵ��Ҫȡ�෴ֵ
		float input[9 * 9 * 6] = { 0 };
		for (int i = 0; i < 81; i++)
		{
			if (node->board[i] == -node->color)
				input[0 * 81 + i] = 1; // ��һͨ��Ϊ�ҷ�����
			if (node->board[i] == node->color)
				input[1 * 81 + i] = 1; // �ڶ�ͨ��Ϊ�Է�����
			if (!node->first)
				input[4 * 81 + i] = 1; // ����ͨ��Ϊ�ҷ��Ƿ�����
		}
		for (auto i : node->available_next)
			input[2 * 81 + i] = 1; // ����ͨ��Ϊ�ҷ���������λ��
		for (auto i : node->available_last)
			input[3 * 81 + i] = 1; // ����ͨ��Ϊ�Է���������λ��
		if (node->last_step >= 0)
			input[5 * 81 + node->last_step] = 1; // ����ͨ��Ϊ�Է����һ��λ��

		int flip = rand() % 2;   // ��ת
		int rotate = rand() % 4; // ��ת

		// ��������з�ת����ת
		if (flip)
			flipData(input, 9, 6);
		if (rotate)
			rotateData(input, 9, 6, rotate);

		float* output = net->predict(input); // ���Ϊ���Ӳ��Ժ��ҷ���ֵ

		// ��������з�����ת����ת
		if (rotate)
			rotateData(output, 9, 1, -rotate);
		if (flip)
			flipData(output, 9, 1);

		memcpy(node->net_policy, output, sizeof(node->net_policy));
		node->net_value = -output[81];
	}

	// �½��ڵ�
	inline TreeNode* newNode(TreeNode* father, int step)
	{
		TreeNode* newNode = new TreeNode();
		memcpy(newNode->board, father->board, sizeof(board));
		newNode->color = -father->color;
		newNode->last_step = step;
		newNode->first = !father->first;
		newNode->board[step] = newNode->color;
		newNode->father = father;
		scanAvailable(newNode);
		predict(newNode);
		father->children[step] = newNode;
		return newNode;
	}

	// ɾ����֧
	void deleteTree(TreeNode* node)
	{
		if (node)
		{
			for (int i = 0; i < 81; i++)
				if (node->children[i])
					deleteTree(node->children[i]);
			delete node;
		}
	}

	// ѡ�������ӽڵ�
	TreeNode* bestChild(TreeNode* node)
	{
		int max_step = 0;
		double max = -std::numeric_limits<double>::max();
		double c = 2.5;
		for (auto step : node->available_next)
		{
			TreeNode* t_node = node->children[step];
			double value = 0;
			if (t_node)
				value = t_node->value / t_node->total + c * node->net_policy[step] * std::sqrt(node->total) / (1 + t_node->total);
			else
				value = c * node->net_policy[step] * std::sqrt(node->total);
			if (value > max)
			{
				max = value;
				max_step = step;
			}
		}
		if (node->children[max_step])
			return node->children[max_step];
		return newNode(node, max_step);
	}

	// ѡ��&ģ��&����
	void select(TreeNode* node)
	{
		// ѡ��
		while (!node->available_next.empty()) // ����ڵ����Ϸû�н���
		{
			node = bestChild(node);
			if (node->is_new)
			{
				node->is_new = false;
				break;
			}
		}

		// ��ֵ
		double value;
		if (node->available_next.empty())
			value = 1;
		else
			value = node->net_value;

		// ����
		while (node)
		{
			node->total += 1;
			node->value += value;
			node = node->father;
			value = -value;
		}
	}

	// ��ʼ����
	void initRoot(int last_step)
	{
		root = new TreeNode();
		root->last_step = last_step;
		root->total = 1;
		memcpy(root->board, board, sizeof(board));
		root->color = OP;
		int count = 0;
		for (int i = 0; i < 81; i++)
		{
			if (judgeAvailable(root->board, i, OP))
				root->available_last.push_back(i);
			if (judgeAvailable(root->board, i, AI))
				root->available_next.push_back(i);
			if (root->board[i])
				count++;
		}
		root->first = count % 2;
		predict(root);
	}

	AlphaPig(Network* net)
	{
		this->net = net;
	}

	int bestStep(double pi)
	{
		auto next_steps = root->available_next;
		constexpr double double_max = std::numeric_limits<double>::max();
		if (pi < std::log(root->total) / std::log(double_max))
		{
			// �������ֵ
			int max_step = 0;
			int max = -1;
			for (auto step : next_steps)
			{
				TreeNode* t_node = root->children[step];
				if (t_node && t_node->total > max)
				{
					max = t_node->total;
					max_step = step;
				}
			}
			return max_step;
		}
		else
		{
			// ����pi�����������
			double total = 0;
			std::vector<std::tuple<int, double>> part;
			for (auto step : next_steps)
			{
				TreeNode* t_node = root->children[step];
				if (t_node)
				{
					double value = std::pow(t_node->total, 1 / pi);
					part.push_back(std::make_tuple(step, value));
					total += value;
				}
			}
			int range = 10000;
			double pos = rand() % range / (double)range;
			double sum = 0;
			for (auto item : part)
			{
				sum += std::get<1>(item) / total;
				if (sum >= pos)
				{
					return std::get<0>(item);
				}
			}
		}
		return -1;
	}

	std::tuple<int, std::vector<float>> trainChoose(int last_step)
	{
		// ��ʼ�����ڵ�
		initRoot(last_step);

		// �ж��Ƿ�����
		if (root->available_next.empty())
		{
			deleteTree(root);
			return std::make_tuple(-1, std::vector<float>());
		}

		// ����
		for (int i = 0; i < 200; i++)
			select(root);

		int count = 0;
		int total = 0;
		std::vector<float> probs;
		for (int i = 0; i < 81; i++)
		{
			if (root->children[i])
			{
				count++;
				total += root->children[i]->total;
			}
		}

		// �������
		for (int i = 0; i < 81; i++)
		{
			if (root->children[i])
				probs.push_back((double)root->children[i]->total / total);
			else
				probs.push_back(0);
		}

		// ѡ����õ��·�
		int best = bestStep(count < 20 ? 1 : 0);

		deleteTree(root);

		return std::make_tuple(best, probs);
	}
};
