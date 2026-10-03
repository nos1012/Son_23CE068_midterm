#include "list.h"
#include "options.h"

#include <locale.h>

int main(int argc, char **argv)
{
    Options options = {0};
    int first_operand;

    setlocale(LC_CTYPE, "");
    if (parse_options(argc, argv, &options, &first_operand) != 0) {
        return 1;
    }
    return list_operands(argc, argv, first_operand, &options);
}
