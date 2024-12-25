#include <string>

namespace glob
{

bool match(const std::string& text, const std::string& wild);
bool glob_match(const std::string& text, const std::string& glob);
bool gitignore_glob_match(const std::string& text, const std::string& glob);

} // namespace glob