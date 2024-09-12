#pragma once

#ifdef __cplusplus
extern "C"
{
#endif
/**
 * Minimal environment management functionality for CSH. 
 * Environment in this context is in where you can manipulate variables,
 * not unlike Shell variables.
 */

    /**
     * @brief callback prototype for function used in csh_foreach_var() function
     */
    typedef void (*csh_foreach_var_cb)(const char *name, void *ctx);

    /**
     * @brief Retrieve the value of an environment variable
     * @param name of the variable to get value for
     * @return pointer to value corresponding to name, NULL if name is not defined in the enviroment
     * @warning the return pointer points to the actual memory holding the value so if you change it,
     * it will affect everybody holding a reference to it. You've been warned
     */
    char *csh_getvar(const char *name);

    /**
     * @brief Create or update an environment variable
     * @param name of the variable to create or update
     * @param value to assign to the variable
     * @return 0 in case of success (variable created or updated). 
     * For now, this is the only possible return value
     * This takes copies of both name & value so you can use stack variables
     * if you wish.
     * 
     */
    int csh_putvar(const char *name, const char *value);
    
    /**
     * @brief Remove an variable from the environment
     * @param name of the variable to remove
     * @return 0 if variable was removed, 1 if it wasn't found in the environment, no harm done, 
     * just letting the caller know
     */
    int csh_delvar(const char *name);

    /**
     * @brief Remove all the variables from the environment
     */
    void csh_clearenv();

    /**
     * @brief Expands references to variables noted as $(VARNAME) in the input string
     * @param input the string containing references to expand
     * @return a pointer to a malloc() 'ed string (remember to free it!).
     * 
     * Variables are referenced in a string like so: $(NAME). No other forms are handled.
     * 
     * If no references are found, an identical copy of the input is returned. It's still a copy so remember to free it.
     * If a variable is not found in the environment, the expanded string will hold no trace of it at all.
     * If a variable is not correctly referenced (for example $NAME), the incorrect reference will appear *verbatim*
     * in the output.
     * If a variable is not correcly closed in terms of parentheses (i.e. there's a "$(" in the input, but no matching ")" is found),
     * the expanded string will stop just before the "$(" appear in the input.
     */
    char *csh_expand_vars(const char *input);

    /**
     * @brief Call the given function for each defined variable in the environment
     * @param cb function to call
     * @param ctx user context that will be passed on to the callback
     */
    void csh_foreach_var(csh_foreach_var_cb cb, void *ctx);

#ifdef __cplusplus
}
#endif
