#include "taskmaster.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

bool add_char(char const *program_name, char const *field_name, char **target, yaml_node_t *value)
{
	bool	ret = true;

	printf("Parsing %s in %s%s\n", field_name,program_name?"program ":"", program_name?program_name:"server");
	if (value->type != YAML_SCALAR_NODE)
	{
		printf("Wrong format for field %s in %s%s, expected a scalar value, encountered %s\n",field_name, program_name?"program ":"", program_name?program_name:"server", value->tag);
	}
	else
	{
		*target = strdup((char *)value->data.scalar.value);
		if (!*target)
		{
			printf("Could not allocate %s in %s%s\n", field_name, program_name?"program ":"", program_name?program_name:"server");
			ret = false;
		}
	}
	return (ret);
}

bool	add_number(char const *program_name, char const *field_name, int *target, yaml_node_t *value, long min, long max)
{
	bool	ret = true;
	long	nb;

	printf("Parsing %s in %s%s\n", field_name,program_name?"program ":"", program_name?program_name:"server");
	if (value->type != YAML_SCALAR_NODE)
	{
		ret = false;
		printf("Wrong format for field %s in %s%s, expected a scalar value, encountered %s\n",field_name, program_name?"program ":"", program_name?program_name:"server", value->tag);
	}
	else
	{
		for (int i = 0; (value->data.scalar.value)[i]; i++)
		{
			if (!isdigit((value->data.scalar.value)[i]))
			{
				ret = false;
				printf("Wrong format for field %s in %s%s, provided value is not a digit, falling back to default value %d\n", field_name,program_name?"program ":"", program_name?program_name:"server", *target);
				break ;
			}
		}
		if (ret)
		{
			nb = strtol((char *)value->data.scalar.value, NULL, 10);
			if (nb < min || nb > max)
			{
				printf("Wrong value for field %s in %s%s, provided value must range between %ld and %ld, falling back to default value %d\n", field_name, program_name?"program ":"", program_name?program_name:"server", min, max, *target);
				ret = false;
			}
			else
			{
				*target = (int)nb;
			}
		}
	}
	return (ret);
}

bool	add_octal(char const *program_name, char const *field_name, int *target, yaml_node_t *value, long min, long max)
{
	bool	ret = true;
	long	nb;

	printf("Parsing %s in %s%s\n", field_name,program_name?"program ":"", program_name?program_name:"server");
	if (value->type != YAML_SCALAR_NODE)
	{
		ret = false;
		printf("Wrong format for field %s in %s%s, expected a scalar value, encountered %s\n",field_name,program_name?"program ":"", program_name?program_name:"server", value->tag);
	}
	else
	{
		for (int i = 0; (value->data.scalar.value)[i]; i++)
		{
			if ((value->data.scalar.value)[i] < '0' || (value->data.scalar.value)[i] > '7')
			{
				ret = false;
				printf("Wrong format for field %s in %s%s, provided value is not a digit in base 8, falling back to default value %d\n", field_name, program_name?"program ":"", program_name?program_name:"server", *target);
				break ;
			}
		}
		if (ret)
		{
			nb = strtol((char *)value->data.scalar.value, NULL, 8);
			if (nb < min || nb > max)
			{
				printf("Wrong value for field %s in %s%s, provided value must range between %ld and %ld, falling back to default value %d\n", field_name, program_name?"program ":"", program_name?program_name:"server", min, max, *target);
				ret = false;
			}
			else
			{
				*target = (int)nb;
			}
		}
	}
	return (ret);
}


void	add_bool(char const *program_name, char const *field_name, bool *target, yaml_node_t *value)
{
	printf("Parsing %s in %s%s\n", field_name, program_name?"program ":"", program_name?program_name:"server");
	if (value->type != YAML_SCALAR_NODE)
	{
		printf("Wrong format for field %s in %s%s, expected a scalar value, encountered %s\n",field_name, program_name?"program ":"", program_name?program_name:"server", value->tag);
	}
	else
	{
		if (!strcmp((char *)value->data.scalar.value, "true")
				|| !strcmp((char *)value->data.scalar.value, "True")
				|| !strcmp((char *)value->data.scalar.value, "on")
				|| !strcmp((char *)value->data.scalar.value, "yes"))
			*target = true;
		else if (!strcmp((char *)value->data.scalar.value, "false")
				|| !strcmp((char *)value->data.scalar.value, "False")
				|| !strcmp((char *)value->data.scalar.value, "off")
				|| !strcmp((char *)value->data.scalar.value, "no"))
			*target = false;
		else
			printf("Wrong format for field %s in %s%s, accepted values are:\n\t- For positive values: true, True, on, yes\n\t- For negative values: false, False, off, no\nfalling back to default value %s\n", field_name, program_name?"program ":"", program_name?program_name:"server", *target?"true":"false");
	}
}
