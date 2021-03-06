#include "json.h"
#include <cstdio>
#include <stack>

struct pack
{
	CFP::json::intern::jsonobj o;
	std::string key;
};

class parse_frame
{
	std::stack<pack> frames;
public:

	CFP::json::intern::jsonobj merge_frames(CFP::json::intern::jsonobj j)
	{
		pack p = frames.top();
		p.o.insert_v(j, p.key);
		frames.pop();
		return p.o;
	}

	void push_frame(std::string key, CFP::json::intern::jsonobj o)
	{
		pack p;
		p.o = o;
		p.key = key;
		frames.push(p);
	}

	CFP::json::intern::jsonobj top()
	{
		return this->frames.top().o;
	}
};

namespace CFP
{
	namespace json
	{
		int CFPjson::deserialize_file(std::string filename, JSONobj& obj)
		{
			FILE* fp = fopen(filename.c_str(), "r");
			if (fp == NULL)
				return -1;
			fseek(fp, 0, SEEK_END);
			size_t len = ftell(fp);
			rewind(fp);

			std::string buf;
			buf.resize(len);

			fread((void*)buf.c_str(), 1, len, fp);

			return this->deserialize(buf, obj);
		}
		int CFPjson::deserialize(std::string buf, JSONobj& obj)
		{
			states s = OBJ;
			size_t layer = 0;

			parse_frame frames;
			intern::jsonobj root(types::VALUE_OBJ);
			
			JSONobj empty;
			obj = empty;

			std::string key;
			std::string val;

			std::string num;

			for (size_t i = 0; i < buf.size(); i++)
			{
				if (layer == 0 && i != 0)
					return 0;
				switch (s)
				{
				case OBJ:
					layer++;
					if (buf[i] != '{')
						return ERR_NO_BASE_OBJ;
					else
						s = WAIT_FOR_KEY;
					break;
				case WAIT_FOR_KEY:
					if (buf[i] == '\"')
						s = KEY;
					else if (isspace(buf[i]));
					else
						return ERR_UNEXPECTED_CHAR;
					break;
				case KEY:
					if (buf[i] == '\"')
						s = WAIT_FOR_COLON;
					else
						key += buf[i];
					break;
				case WAIT_FOR_COLON:
					if (buf[i] == ':')
						s = WAIT_FOR_VALUE;
					else if (isspace(buf[i]));
					else
						return ERR_UNEXPECTED_CHAR;
					break;
				case WAIT_FOR_VALUE:
					if (buf[i] == '\"')
						s = STRING_VALUE;
					else if (isdigit(buf[i]) || buf[i] == '-' || buf[i] == '+')
					{
						s = NUMERIC_VALUE;
						i--;
					}
					else if (buf[i] == '[')
					{
						layer++;
						frames.push_frame(key, root);
						key.clear();
						root.clear();
						root.get_type() = types::VALUE_ARRAY;
						s = ARRAY_WAIT_FOR_VALUE;
					}
					else if (buf[i] == '{')
					{
						layer++;
						frames.push_frame(key, root);
						key.clear();
						root.clear();
						root.get_type() = types::VALUE_OBJ;
						s = WAIT_FOR_KEY;
					}
					else if (isspace(buf[i]));
					else
						return ERR_UNEXPECTED_CHAR;
					break;
				case ARRAY_WAIT_FOR_VALUE:
					if (buf[i] == '\"')
						s = ARRAY_STRING_VALUE;
					else if (isdigit(buf[i]) || buf[i] == '-' || buf[i] == '+')
					{
						s = ARRAY_NUMERIC_VALUE;
						i--;
					}
					else if (buf[i] == '[')
					{
						layer++;
						frames.push_frame(key, root);
						key.clear();
						root.clear();
						root.get_type() = types::VALUE_ARRAY;
						s = ARRAY_WAIT_FOR_VALUE;
					}
					else if (isspace(buf[i]));
					else
						return ERR_UNEXPECTED_CHAR;
					break;
				case ARRAY_NUMERIC_VALUE:
					if (isdigit(buf[i]) || buf[i] == '-' || buf[i] == '+' || buf[i] == 'e' || buf[i] == 'E' || buf[i] == '.')
						num += buf[i];
					else if (isspace(buf[i]))
					{
						intern::jsonobj value_num;
						if (intern::convert_numeric(num, value_num) != 0)
							return -1;
						root.insert_v(value_num, key);
						num.clear();
						key.clear();
						s = ARRAY_WAIT_FOR_COMMA;
					}
					else if (buf[i] == ',')
					{
						intern::jsonobj value_num;
						if (intern::convert_numeric(num, value_num) != 0)
							return -1;
						root.insert_v(value_num, key);
						num.clear();
						key.clear();
						s = ARRAY_WAIT_FOR_VALUE;
					}
					break;
				case STRING_VALUE:
					if (buf[i] == '\"')
					{
						s = WAIT_FOR_COMMA;
						root.insert_v(val, key);
						val.clear();
					}
					else
						val += buf[i];
					break;
				case WAIT_FOR_COMMA:
					if (buf[i] == ',')
						s = WAIT_FOR_KEY;
					else if (buf[i] == '}')
					{
						layer--;
						if (layer != 0)
							root = frames.merge_frames(root);
					}
					else if (buf[i] == ']')
						return ERR_BRACKETS_MISMATCH;
					else if (isspace(buf[i]));
					else
						return ERR_UNEXPECTED_CHAR;
					break;
				case ARRAY_WAIT_FOR_COMMA:
					if (buf[i] == ',')
						s = ARRAY_WAIT_FOR_VALUE;
					else if (buf[i] == ']')
					{
						layer--;
						if (layer != 0)
							root = frames.merge_frames(root);
						if (root.get_type() == types::VALUE_ARRAY)
							s = ARRAY_WAIT_FOR_COMMA;
						else if(root.get_type() == types::VALUE_OBJ)
							s = WAIT_FOR_COMMA;
					}
					else if (buf[i] == '}')
						return ERR_BRACKETS_MISMATCH;
					else if (isspace(buf[i]));
					else
						return ERR_UNEXPECTED_CHAR;
					break;
				case NUMERIC_VALUE:
					if (isdigit(buf[i]) || buf[i] == '-' || buf[i] == '+' || buf[i] == 'e' || buf[i] == 'E' || buf[i] == '.')
						num += buf[i];
					else if (isspace(buf[i]))
					{
						intern::jsonobj value_num;
						if (intern::convert_numeric(num, value_num) != 0)
							return -1;
						root.insert_v(value_num, key);
						num.clear();
						key.clear();
						s = WAIT_FOR_COMMA;
					}
					else if (buf[i] == ',')
					{
						intern::jsonobj value_num;
						if (intern::convert_numeric(num, value_num) != 0)
							return -1;
						root.insert_v(value_num, key);
						num.clear();
						key.clear();
						s = WAIT_FOR_KEY;
					}
					else
						return ERR_UNEXPECTED_CHAR;
					break;
				case ARRAY_STRING_VALUE:
					if (buf[i] == '\"')
					{
						s = ARRAY_WAIT_FOR_COMMA;
						root.insert_v(val, key);
						val.clear();
					}
					else
						val += buf[i];
				}
				if (layer == 0)
					break;
			}


			if (layer != 0)
				return ERR_BRACKETS_MISMATCH;
			obj = root;
			return 0;
		}
	}
}