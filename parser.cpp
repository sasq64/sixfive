
#include <boost/spirit.hpp>
#include <unordered_map>
#include <stack>
#include <string>

using namespace boost::spirit;

struct AsmState
{
	std::unordered_map<std::string, double> symbols;
	std::vector<std::string> undefined;
};

struct AsmGrammar : public boost::spirit::grammar<AsmGrammar>
{
	//int encode(const std::string &op, const std::string &arg) const {
	//	return encoder(op, arg);
	//};

	std::function<int(uint16_t org, const std::string &op, const std::string &arg)> encode;

	AsmGrammar(AsmState &as) : state(as) {}

	AsmState& state;


	template <typename ScannerT> struct definition
	{
		using Rule = boost::spirit::rule<ScannerT>;

		std::unordered_map<std::string, double> &symbols;
		std::vector<std::string> &undefined;

		definition(AsmGrammar const &g) : grammar(g), symbols(g.state.symbols), undefined(g.state.undefined) {
			symbols["PI"] = M_PI;
			s0_ap = constant_ap[fpushnum] | ('(' >> s6_ap >> ')');
		}

		using FnN = std::function<void(int)>;
		using FnC = std::function<void(char)>;
		using FnD = std::function<void(double)>;
		using Fn = std::function<void(const char *, const char *)>;

		std::unordered_map<std::string, std::function<double(double, double)>> ops = {
			{ "+", [](double a, double b) -> double { return a + b; } },
			{ "-", [](double a, double b) -> double { return a - b; } },
			{ "*", [](double a, double b) -> double { return a * b; } },
			{ "/", [](double a, double b) -> double { return a / b; } },
			{ ">>", [](double a, double b) -> double { return (unsigned)a >> (unsigned)b; } }
		};

		const AsmGrammar& grammar;

		std::stack<double> valstack;
		std::string opcodeName;
		std::string labelName;
		std::string symbolName;
		std::string argExp;
		std::string opcodeArg;
		double expValue;
		uint32_t org = 0;

		struct
		{
			bool negate = false;
			double val;
		} e;

		FnC fnegate = [=](const char) { e.negate = true; };
		FnD fsetnum = [=](double n) { e.val = n; };

		Fn fpushnum = [=](auto, auto) {
			valstack.push(e.negate ? -e.val : e.val);
			e.negate = false;
			e.val = 1234.5678;
		};

		Fn fop = [=](auto a, auto b) { 
			auto op = std::string(a, a+1);
			auto v1 = valstack.top();
			valstack.pop();
			auto v0 = valstack.top();
			valstack.pop();
			valstack.push(ops[op](v0, v1));
		};

		Fn fexpression = [=](auto a, auto b) {
			printf("STACK: %f\n", valstack.top());
			expValue = valstack.top();
			valstack.pop();
		};

		Fn fsym = [=](auto a, auto b) {
			std::string s(a, b);
			printf("SYM '%s'\n", s.c_str());
			if(s == "*" || s == "$") {
				e.val = org;
				return;
			}

			auto it = symbols.find(s);
			if(it == symbols.end())
				undefined.push_back(s);
			else
				e.val = it->second;
		};

		Fn flabel = [=](auto a, auto b) {
			while(b[-1] == ':') b--;
			std::string label(a, b);
			printf("LABEL %s\n", label.c_str());
			symbols[label] = org;
		};

		Fn fasmcode = [=](auto a, auto b) {
			//printf("OPCODE %s\n", opcodeName.c_str());
		};


		Fn fopcode = [=](auto a, auto b) {
			opcodeName = std::string(a,b);
			opcodeArg = "";
		};


		Fn foparg = [=](auto a, auto b) {
			std::string arg(a, b);

			printf("Replacing '%s' in '%s' with %d\n", argExp.c_str(), arg.c_str(), (int)expValue);

			auto pos = arg.find(argExp);
			arg.replace(pos, argExp.length(), std::to_string((long long)expValue));
			auto e = std::remove(arg.begin(), arg.end(), ' ');
			*e = 0;
			std::transform(arg.begin(), arg.end(), arg.begin(), ::tolower);
			opcodeArg = arg;
		};

		Fn fasmline = [=](auto, auto) {
			//if(labelName != "")
			//	symbols[labelName] = org;
			//labelName = "";
			std::transform(opcodeName.begin(), opcodeName.end(), opcodeName.begin(), ::tolower);
			org += grammar.encode(org, opcodeName, opcodeArg);
			//printf("ASMLINE\n");
		};

		Fn femptyline = [=](auto, auto) {
			//if(labelName != "")
			//	symbols[labelName] = org;
			//labelName = "";
		};

		Fn fdefline = [=](auto, auto) {
			if(symbolName == "*" || symbolName == "$") {
				org = expValue;
				return;
			}

			symbols[symbolName] = expValue;
			printf("Assign '%f' to '%s'\n", expValue, symbolName.c_str());
		};
//
		Fn SetMe(std::string &target) {
			return [&](const char *a, const char *b) {
				target = std::string(a, b);
			};
		};

		Rule symbol_ap = ch_p('*') | ch_p('$') | lexeme_d[ (ch_p('_') | alpha_p) >> *(ch_p('_') | alnum_p) ] ;

		Rule constant_ap =
		    lexeme_d[!ch_p('-')[fnegate] >>
		             (((str_p("0x") | ch_p('$')) >> hex_p[fsetnum]) |
		              (ch_p('%') >> bin_p[fsetnum]) | real_p[fsetnum] |
		              ( (ch_p('$') | (  (ch_p('_') | alpha_p) >> *(ch_p('_') | alnum_p) ) )[fsym] | uint_p[fsetnum]) |
		              (ch_p('\'') >> anychar_p[fsetnum] >> ch_p('\'')))];

		// Moved to constructor
		//Rule s0_ap = constant_ap[fpushnum] | ('(' >> s6_ap >> ')');
		Rule s0_ap;

		Rule s1_ap = s0_ap >> *(('*' >> s0_ap)[fop] | ('/' >> s0_ap)[fop] |
		                        ('%' >> s0_ap)[fop]);

		Rule s2_ap = s1_ap >> *(('+' >> s1_ap)[fop] | ('-' >> s1_ap)[fop]);

		Rule s3_ap = s2_ap >> *((">>" >> s2_ap)[fop] | ("<<" >> s2_ap)[fop]);

		Rule s4_ap = s3_ap >> *(('>' >> s3_ap)[fop] | ('<' >> s3_ap)[fop] |
		                        ("==" >> s3_ap)[fop] | (">=" >> s3_ap)[fop] |
		                        ("<=" >> s3_ap)[fop] | ("!=" >> s3_ap)[fop]);

		Rule s5_ap = s4_ap >> *(('|' >> s4_ap)[fop] | ('&' >> s4_ap)[fop] |
		                        ('^' >> s4_ap)[fop]);

		Rule s6_ap = s5_ap >> *(("||" >> s5_ap)[fop] | ("&&" >> s5_ap)[fop]);

		Rule expression_ap = s6_ap[fexpression];


		Rule opcode_ap = lexeme_d[ as_lower_d[ alpha_p >> *('_' | alnum_p) ] ];
		Rule reg_p = as_lower_d[ ch_p('x') | ch_p('y') ];

		Rule oparg_ap = ('(' >> expression_ap[SetMe(argExp)] >> !(',' >> reg_p) >> ')'>>  !(',' >> reg_p)) |
		   	(!ch_p('#') >> expression_ap[SetMe(argExp)] >> !(',' >> reg_p)) ;

		Rule asmcode_ap = opcode_ap[fopcode] >> !oparg_ap[foparg] ;

		Rule label_ap = lexeme_d[ (ch_p('.') | ch_p('_') | alpha_p) >> *(ch_p('_') | alnum_p) >> ch_p(':') ];

		Rule asmline_ap = !label_ap[flabel] >> asmcode_ap[fasmcode] >> (comment_p(";") | eol_p);
		Rule emptyline_ap = !label_ap[flabel] >> (comment_p(";") | eol_p);

		Rule defline_ap = symbol_ap[SetMe(symbolName)] >> '=' >> expression_ap >> (comment_p(";") | eol_p);

		Rule mainrule_ap = *(emptyline_ap[femptyline] | asmline_ap[fasmline] | defline_ap[fdefline]);

		Rule const &start() const { return mainrule_ap; }
	};
};

bool parse(const std::string &code, 
		std::function<int(uint16_t org, const std::string &op, const std::string &arg)> encode) {

	AsmState state;

	AsmGrammar g(state);
	g.encode = encode;
	int ucount = -1;
	while(true) {
		auto result = boost::spirit::parse((code + "\n").c_str(), g, blank_p);
		if(!result.full)
			return false;

		if(state.undefined.size() > 0) {
			if(ucount >= 0 && (int)state.undefined.size() >= ucount) {
				printf("Error\n");
				for(const auto &ud : state.undefined)
					printf("'%s' is undefined\n", ud.c_str());
				return false;
			}
			ucount = state.undefined.size();
			state.undefined.clear();
		} else
			return true;

	}
	//return result.full;
}
