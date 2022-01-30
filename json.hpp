#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <ostream>
#include <stdexcept>
#include <string.h>


template<typename T, typename... Ts>
concept isAnyOf = (std::same_as<T, Ts> || ...);

/**
 * @brief a union that keeps track of its dynamic type and manages its own data
 * 
 * @tparam E 			an enum for the types  
 * @tparam NULL_TYPE 	the enum tape for 'null' or 'undefined' 
 * @tparam Ts 			the dynamic types
 */
template<typename E, E NULL_TYPE, typename... Ts>
class smartUnion {
	static_assert(std::is_enum<E>::value, "Must be an enum type");

private:
	uint64_t data;

	/**
	 * @brief finds the index of a type in the unions type parameter list 
	 * 
	 * @tparam T 				the type to be mapped
	 * @return constexpr size_t the index
	 */
	template <typename T>
	static constexpr size_t find_index() {
		const constexpr bool matches[sizeof...(Ts)]{
			std::is_same<T, Ts>::value...
		};
		size_t index = -1;
		for (size_t i = 0; i < sizeof...(Ts); i++) {
			if (matches[i]) {
				if (index != -1) {
					return -2;
				}
				index = i;
			}
		}
		return index;
	}

	template<typename T>
	static E find_type() {
		const constexpr size_t idx = find_index<T>();
		static_assert(idx != -1, "use of unknown type");
		static_assert(idx != -2, "use of ambiguous type");
		return (E) (idx + 1);
	}

	/**
	 * @brief determines whether an object is stored on the heap or locally
	 * 
	 * @param t 		The enum of a certain type
	 * @return true 	corresponding type is to big for local storage
	 * @return false 	corresponding type fits in local storage
	 */
	static constexpr bool using_pointer(E t) {
		constexpr bool use_pointer[sizeof...(Ts)]{ (sizeof(Ts) > sizeof(void*))... };
		return t == NULL_TYPE ? false : use_pointer[((size_t)t) - 1];
	}

public:
	E type;

	smartUnion() : data{ 0 }, type{ NULL_TYPE } {}

	template<typename T>
	smartUnion(const T& t) requires isAnyOf<T, Ts...> {
		type = find_type<T>();
		if constexpr (sizeof(T) > sizeof(void*)) {
			data = (uint64_t) new T(t);
		} else {
			data = *(uint64_t*)&t;
		}
	}

	template<typename T>
	smartUnion(T&& t) requires isAnyOf<T, Ts...> {
		type = find_type<T>();
		if constexpr (sizeof(T) > sizeof(void*)) {
			data = (uint64_t) new T(std::move(t));
		} else {
			data = *(uint64_t*)&t;
		}
	}

	smartUnion(smartUnion<E,  NULL_TYPE, Ts...>&& otherUnion) {
		type = otherUnion.type;
		data = otherUnion.data;
		if (using_pointer(type)) {
			otherUnion.type = NULL_TYPE;
			otherUnion.data = 0;
		}
	}

	~smartUnion() {
		if (using_pointer(type)) {
			delete (char*)data;
		}
	}


	template<typename T>
	smartUnion<E, NULL_TYPE, Ts...>& operator=(const T& t) requires isAnyOf<T, Ts...> {
		E newType = find_type<T>();
		if constexpr (sizeof(T) > sizeof(void*)) {
			if (newType == type) {
				((T*)data)->operator=(t);
			} else {
				if (using_pointer(type))
					delete (char*) data;
				data = (uint64_t) new T(t);
			}
		} else {
			if (using_pointer(type))
					delete (char*) data;
			data = *(uint64_t*)&t;
		}
		type = newType;
		return *this;
	}

	template<typename T>
	smartUnion<E, NULL_TYPE, Ts...>& operator=(T&& t) requires isAnyOf<T, Ts...> {
		E newType = find_type<T>();
		if constexpr (sizeof(T) > sizeof(void*)) {
			if (newType == type) {
				((T*)data)->operator=(t);
			} else {
				if (using_pointer(type))
					delete (char*) data;
				data = (uint64_t) new T(std::move(t));
			}
		} else {
			if (using_pointer(type))
				delete (char*) data;
			data = *(uint64_t*)&t;
		}
		type = newType;
		return *this;
	}
		
	smartUnion<E, NULL_TYPE, Ts...>& operator=(smartUnion<E,  NULL_TYPE, Ts...>&& otherUnion) {
		if (using_pointer(type))
			delete (char*) data;
		data = otherUnion.data;
		type = otherUnion.type;
		otherUnion.type = NULL_TYPE;
		otherUnion.data = 0;
		return *this;
	}

	template<typename T>
	const T& get() const requires isAnyOf<T, Ts...> {
		using namespace std::string_literals;
		E tType = find_type<T>();
		if (tType != type)
			throw std::invalid_argument(
				"wrong type T: "s + std::to_string((int)tType) +
				" type: "s +  std::to_string((int)type)
			);
		if constexpr (sizeof(T) > sizeof(void*)) {
			return *(T*)data;
		} else {
			return *(T*)&data;
		}
	}

	template<typename T>
	T& get() requires isAnyOf<T, Ts...> {
		E tType = find_type<T>();
		if (tType!= type)
			throw std::invalid_argument(
				"wrong type T: "s + std::to_string((int)tType) +
				" type: "s +  std::to_string((int)type)
			);
		if constexpr (sizeof(T) > sizeof(void*)) {
			return *(T*)data;
		} else {
			return *(T*)&data;
		}
	}

	/**
	 * @brief Creates a copy with the help of 
	 * 
	 * @tparam T 			a type correspnding to the enum value 
	 * @return smartUnion 	a copy of this {@link smartUnion}
	 */
	template<typename T>
	smartUnion copy() const requires isAnyOf<T, Ts...> {
		E tType = find_type<T>();
		if (tType != type)
			throw std::invalid_argument(
				"wrong type T: "s + std::to_string((int)tType) +
				" type: "s +  std::to_string((int)type)
			);
		return smartUnion(std::move(T(get<T>())));
	}
};

class json;

/**
 * @brief typedefs of data types to make implementation more flexible and expressive
 * 
 */
typedef bool Boolean;
typedef double Number;
typedef std::string String;
typedef std::vector<json> Array;
typedef std::unordered_map<std::string, json> Object;

template<class T>
concept json_data_type = isAnyOf<T, Boolean, Number, String, Array, Object>;

class json {
public:
	enum class json_type : uint8_t {
		null,
		boolean,
		number,
		string,
		array,
		object
	};

	static std::string typeToString(json_type type) {
		switch (type) {
		using enum json_type;
		case null:	return "null";
		case boolean:	return "boolean";
		case number:	return "number";
		case string:	return "string";
		case array:		return "array";
		case object:	return "object";
		default: throw std::runtime_error("Invalid json type");
		}
	}


private:
	typedef smartUnion<json_type, json_type::null, Boolean, Number, String, Array, Object> json_data;

	json_data data;

public:

	//----------------------[ constructors ]---------------------//
	
	json() = default;

	template<json_data_type T>
	json(const T& newData) : data(newData) {}

	template<json_data_type T>
	json(T&& newData) : data(std::move(newData)) {}

	
	json(const json& otherJSON) : data(copy_json_data(otherJSON.data)) {}

	json(json&& otherJSON) : data(std::move(otherJSON.data)) {}

	static json_data copy_json_data(const json_data& data) {
		switch (data.type) {
		using enum json_type;
		case boolean:	return data.copy<Boolean>();
		case number:	return data.copy<Number>();
		case string:	return data.copy<String>();
		case array:		return data.copy<Array>();
		case object:	return data.copy<Object>();
		default: return json_data();
		}
	}

	//----------------------[ parsing ]---------------------//

	static json parse(const std::string& txt) {
		if (txt.length() < 2)
			throw std::runtime_error("Invalid json (empty string)");

		size_t index = 0;
		if (txt[0] != '{' && txt[0] != '[')
			skipSpaces(txt, index);

		if (txt[index] == '{') {
			return json::parseObject(txt, index);
		} else if (txt[index] == '[') {
			return json::parseArray(txt, index);
		} else {
			throw std::runtime_error("Invalid json");
		}
	}

private:
	inline static void skipSpaces(const std::string& txt, size_t& index) {
		while (++index < txt.length()) {
			if (!std::isspace(txt[index])) {
				break;
			}
		}
	}

	typedef json (*parser)(const std::string& txt, size_t& index);

	static const parser getParser(const char begin) {
		switch (begin) {
			using namespace std::string_literals;
			case '{':			return &json::parseObject;
			case '[':			return &json::parseArray;
			case '\"':			return &json::parseString;
			case 't':
			case 'f':			return &json::parseBoolean;
			case '0' ... '9':	return &json::parseNumber;
			case 'n':			return &json::parseNull;
			default: throw std::runtime_error("Invalid symbole begin: "s + begin);
		}
	}

	static json parseNull(const std::string& txt, size_t& index) {
		if (txt.length() > index + 3 && !strncmp(&txt[index], "null", 4)) {
			index += 3;
			return json();
		} else {
			throw parsingError(txt, index);
		}
	}

	static json parseBoolean(const std::string& txt, size_t& index)  {
		if (index < txt.length()) {
			if (strncmp(&txt[index], "false", 5) == 0) {
				index += 4;
				return json(false);
			} else if (strncmp(&txt[index], "true", 4) == 0) {
				index += 3;
				return json(true);
			}
		}
		throw parsingError(txt, index);
	}
	
	static json parseNumber(const std::string& txt, size_t& index) {
		std::size_t parsedChars = 0;
		double data = std::stod(txt.substr(index), &parsedChars);
		index += parsedChars - 1;
		return json(data);
	}

	static json parseString(const std::string& txt, size_t& index) {
		std::string data;
		while (txt[++index] != '\"') {
			data += txt[index];
			if (index + 2 >= txt.length()) {
				throw parsingError(txt, index);
			}
		}
		return json(std::move(data));
	}

	static json parseArray(const std::string& txt, size_t& index) {
		skipSpaces(txt, index);
		const parser f = getParser(txt[index]);
		index--;
		Array data;
		do {
			skipSpaces(txt, index);
			data.push_back(f(txt, index));
			skipSpaces(txt, index);
		} while (txt[index] == ',' && index < txt.length());

		return json(std::move(data));
	}

	static json parseObject(const std::string& txt, size_t& index) {
		Object data;
		do {
			skipSpaces(txt, index);

			if(txt[index] == '}')
				return json(data);

			std::string name;
			for (++index; index < txt.length() && txt[index] != '\"'; index++) {
				name += txt[index];
			}

			skipSpaces(txt, index);
			skipSpaces(txt, index);

			data.insert({ name, getParser(txt[index])(txt, index) });

			skipSpaces(txt, index);

		} while (txt[index] == ',' && index < txt.length());

		return json(std::move(data));
	}

	//----------------------[ errors ]---------------------//
	
	const std::runtime_error castError(const json_type t) const {
		using std::operator""s;
		return std::runtime_error(
			"Tried to cast json value of type '"s + typeToString(data.type) + 
			 "' to '"s + typeToString(t) + '\''
		);
	}

	static const std::runtime_error parsingError(const std::string& txt, const size_t index) {
		using std::operator""s;
		return std::runtime_error(
			"Invalid symbole '"s + txt[index] + "' at index "s +
			std::to_string(index) + '\''
		);
	}

public:
	//----------------------[ accesors ]---------------------//

	const json& operator[](const size_t index) const {
		using std::operator""s;
		if (data.type == json_type::array) {
			size_t length = data.get<Array>().size();
			if (index < length) {
				return data.get<Array>()[index];
			} else throw std::out_of_range("index: "s + std::to_string(index) + " length: "s + std::to_string(length));
		} else throw castError(json_type::array);
	}

	json& operator[](const size_t index) {
		using std::operator""s;
		if (data.type == json_type::array) {
			size_t length = data.get<Array>().size();
			if (index < length) {
				return data.get<Array>()[index];
			} else throw std::out_of_range("index: "s + std::to_string(index) + " length: "s + std::to_string(length));
		} else throw castError(json_type::array);
	}


	const json& operator[](const char* s) const { return data.get<Object>().at(s); }
	const json& operator[](const std::string& s) const { return data.get<Object>().at(s); }

	json& operator[](const char* s) { return data.get<Object>()[s]; }
	json& operator[](const std::string& s) { return data.get<Object>()[s]; }

	json_type getType() const { return data.type; };

	size_t size() {
		switch (data.type) {
			using enum json_type;
			case array:		return data.get<Array>().size();
			case object:	return data.get<Object>().size();
			default: throw castError(json_type::array);
		}
	}

	size_t length() {
		if (data.type == json_type::string)
			return data.get<String>().length();
		else throw castError(json_type::string);
	}

	//----------------------[ to_string ]---------------------//
	
	friend std::ostream & operator<<(std::ostream&, const json&);

	void to_string(std::ostream& out, int indent = -1) const {
		static const constexpr char tab = '\t';
		using enum json_type;

		if (data.type == null) {
			out << "null";
		} else if (data.type == boolean) {
			out << (data.get<Boolean>() ? "true" : "false");
		} else if (data.type == number) {
			out <<  std::to_string(data.get<Number>());
		} else if (data.type == string) {
			out <<  "\"" << data.get<String>() << "\"";
		} else {
			const std::string indentTabs = (indent < 0 ? "" : std::string(++indent, tab));
			const std::string lineBreak = (indent < 0 ? "" : "\n");
			
			if (data.type == array) {
				out  << "[" << lineBreak;
				auto it = data.get<Array>().begin();
				const auto end = data.get<Array>().end();
				while (it != end) {
					out << indentTabs;
					it->to_string(out, indent);
					if (++it == end) {
						out << lineBreak;
						break;
					} else out << "," << lineBreak;
				}
				out << (indent < 0 ? "" : std::string(--indent, tab)) << "]";
			} 
			else if (data.type == object) {
				out << "{" << lineBreak;
				auto it = data.get<Object>().begin();
				const auto end = data.get<Object>().end();
				while (it != end) {
					out << indentTabs << "\"" << it->first << "\": ";
					it->second.to_string(out, indent);
					if (++it == end) {
						out << lineBreak;
						break;
					}
					else out << "," << lineBreak;
				}
				out << (indent < 0 ? "" : std::string(--indent, tab)) << "}";
			}
		}
	}


	//----------------------[ assignemt ]---------------------//

	template<json_data_type T>
	json& operator=(const T& t) {
		data = t;
		return *this;
	}

	template<json_data_type T>
	json& operator=(T&& t) {
		data = std::move(t);
		return *this;
	}

	json& operator=(const json& otherJSON) {
		data = std::move(copy_json_data(otherJSON.data));
		return *this;
	}

	json& operator=(json&& otherJSON) {
		//data = std::move(otherJSON.data);
		data.operator=((json_data&&)otherJSON.data);
		return *this;
	}
	
	//----------------------[ casts ]---------------------//
	
	template<json_data_type T>
	operator const T() const {
		return data.get<T>();
	}

	template<json_data_type T>
	operator const T&() const {
		return data.get<T>();
	}

	template<json_data_type T>
	operator T&() {
		return data.get<T>();
	}
};

std::ostream& operator<<(std::ostream& os, const json& json) {
	json.to_string(os, 0);
	return os;
}

