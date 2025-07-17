#include <stdio.h>
#include <stdbool.h>
#include <dlfcn.h>
#include <pwd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <signal.h>
#include <apm/csh_api.h>

#define PY_SSIZE_T_CLEAN
#include <Python.h>

#include <slash/slash.h>
#include <slash/optparse.h>
#include <slash/completer.h>
#include <pycsh/utils.h>
#include "python_loader.h"
#include "walkdir.h"
#include "slash_apm.h"

#define PYAPMS_DIR "/.local/lib/csh/"
#define DEFAULT_INIT_FUNCTION "apm_init"

PyThreadState *main_thread_state = NULL;

bool exception_allowed = false;

/**
 * @brief Clear any KeyboardInterrupts which were raised between slash command execution.
 * 
 * i.e, on an empty prompt.
 */
void on_python_slash_execute_pre_hook(const char *line) {

	if (!Py_IsInitialized()) {
		// If Python is not initialized, we cannot do anything.
		return;
	}
	
	PyGILState_STATE CLEANUP_GIL gstate = PyGILState_Ensure();
	if (PyErr_Occurred()) {
		assert(PyErr_ExceptionMatches(PyExc_KeyboardInterrupt));
		PyErr_Clear();
	}

	exception_allowed = true;
}

/**
 * Handle/Print any exceptions which were raised during slash command execution.
 * With Python being embedded in CSH, there is no outer logic to handle exceptions.
 */
void on_python_slash_execute_post_hook(const char *line, struct slash_command *command) {

	if (!Py_IsInitialized()) {
		// If Python is not initialized, we cannot do anything.
		return;
	}
	
	PyGILState_STATE CLEANUP_GIL gstate = PyGILState_Ensure();
	if (PyErr_Occurred()) {
		if (PyErr_ExceptionMatches(PyExc_KeyboardInterrupt)) {
			/* This is a post-hook. There is nothing to interrupt, so simply clear the error. */
			PyErr_Clear();
		} else {
			PyErr_Print();
		}
	}

	exception_allowed = false;
}

static void _dlclose_cleanup(void *const* handle) {
	if (*handle == NULL || handle == NULL) {
		return;
	}

	dlclose(*handle);
}

static PyObject * pycsh_integrate_pymod(const char * const _filepath) {
	void *handle __attribute__((cleanup(_dlclose_cleanup))) = dlopen(_filepath, RTLD_NOW | RTLD_GLOBAL);
    if (!handle) {
        fprintf(stderr, "Error loading shared object file: %s\n", dlerror());
        return NULL;
    }

    // Construct the initialization function name: PyInit_<module_name>
    char * const filepath CLEANUP_STR = safe_strdup(_filepath);
	if(filepath == NULL) {
		return NULL ;
	}
	char *filename = strrchr(filepath, '/');
    if (!filename) {
        filename = filepath;
    } else {
        filename++;
    }
    char *dot = strchr(filename, '.');
    if (dot) {
        *dot = '\0';
    }

    size_t init_func_name_len = strlen("PyInit_") + strlen(filename) + 1;
    char init_func_name[init_func_name_len];
    snprintf(init_func_name, init_func_name_len, "PyInit_%s", filename);

    typedef PyObject* (*PyInitFunc)(void);
    PyInitFunc init_func = (PyInitFunc)dlsym(handle, init_func_name);

    if (!init_func) {
        fprintf(stderr, "Error finding initialization function: %s\n", dlerror());
        return NULL;
    }

    PyObject *module AUTO_DECREF = init_func();
    if (!module) {
        PyErr_Print();
        return NULL;
    }

#if 1
	/* TODO Kevin: Are we responsible for adding to sys.modules?
		Or will PyModule_Create() do that for us? */

	PyObject *module_name AUTO_DECREF = PyObject_GetAttrString(module, "__name__");
	assert(module_name != NULL);

	//const char *module_name_str = PyUnicode_AsUTF8(module_name);
	//assert(module_name_str != NULL);

    // Add the module to sys.modules
    if (PyDict_SetItem(PyImport_GetModuleDict(), module_name, module) < 0) {
        return NULL;
    }
#endif

	/* Currently init_func() will be called and executed successfully,
		but attempting to PyObject_GetAttrString() on the returned module causes a segmentation fault.
		For future reference, here is some reading material on how Python handles the import on Unixes:
		- https://stackoverflow.com/questions/25678174/how-does-module-loading-work-in-cpython */
	Py_INCREF(module);
    return module;
}

PyObject * pycsh_load_pymod(const char * const _filepath, const char * const init_function_name, int verbose) {

	if (_filepath == NULL) {
		return NULL;
	}

	char * const filepath CLEANUP_STR = safe_strdup(_filepath);
	if(filepath == NULL) {
		return NULL;
	}
	char *filename = strrchr(filepath, '/');

	/* Construct Python import string */
	if (filename == NULL) {  // No slashes in string, maybe good enough for Python
		filename = filepath;
	} else if (*(filename + 1) == '\0') {   // filename ends with a trailing slash  (i.e "dist-packeges/my_package/")
		*filename = '\0';  				    // Remove trailing slash..., (i.e "dist-packeges/my_package")
		filename = strrchr(filepath, '/');  // and attempt to find a slash before it. (i.e "/my_package")
		if (filename == NULL) {			    // Use full name if no other slashes are found.
			filename = filepath;
		} else {						    // Otherwise skip the slash. (i.e "my_package")
			filename += 1;
		}
	} else {
		filename += 1;
	}

	if (strcmp(filename, "__pycache__") == 0) {
		return NULL;
	}
	if (*filename == '.') {  // Exclude hidden files, . and ..
		return NULL;
	}
	
	char *dot = strchr(filename, '.');
	if (dot != NULL) {
		*dot = '\0';
	}

	{	/* Python code goes here */
		//printf("Loading script: %s\n", filename);

		PyObject *pName AUTO_DECREF = PyUnicode_DecodeFSDefault(filename);
		if (pName == NULL) {
			PyErr_Print();
			fprintf(stderr, "Failed to decode script name: %s\n", filename);
			return NULL;
		}

		PyObject *pModule AUTO_DECREF;

		/* Check for .so extension */
		int filepath_len = strlen(_filepath);
		if (filepath_len >= 3 && strcmp(_filepath + filepath_len - 3, ".so") == 0) {
			/* Assume this .so file is a Python module that should link with us.
				If it isn't a Python module, then this call will fail,
				after which the original "apm load" will attempt to import it.
				If it is an independent module, that should link with us,
				then is should've been imported with a normal Python "import" ya dummy :) */
			pModule = pycsh_integrate_pymod(_filepath);
		} else {
			pModule = PyImport_Import(pName);
		}

		if (pModule == NULL) {
			//PyErr_Print();
			//fprintf(stderr, "Failed to load module: %s\n", filename);
			return NULL;
		}

		if (init_function_name == NULL) {
			if (verbose >= 2) {
				printf("Skipping init function for module '%s'\n", filename);
			}
			Py_INCREF(pModule);
			return pModule;
		}

		// TODO Kevin: Consider the consequences of throwing away the module when failing to call init function.

		assert(!PyErr_Occurred());
		PyObject *init_function AUTO_DECREF = PyObject_GetAttrString(pModule, init_function_name);
		if (init_function == NULL) {
			PyErr_Clear();
			if (verbose >= 2) {
				fprintf(stderr, "Skipping missing init function '%s()' in module '%s'\n", init_function_name, filename);
			}
			Py_INCREF(pModule);
			return pModule;
		}

		if (!PyCallable_Check(init_function)) {
			PyErr_Format(PyExc_TypeError, "init function '%s()' for module '%s' is not callable");
			return NULL;
		}

		//printf("Preparing arguments for script: %s\n", filename);

		PyObject *pArgs AUTO_DECREF = PyTuple_New(0);
		if (pArgs == NULL) {
			PyErr_Print();
			fprintf(stderr, "Failed to create arguments tuple\n");
			return NULL;
		}

		PyObject *pValue AUTO_DECREF = PyObject_CallObject(init_function, pArgs);

		if (pValue == NULL) {
			PyErr_Print();
			fprintf(stderr, "Call failed for: %s.%s\n", filename, init_function_name);
			return NULL;
		}

		if (verbose >= 2) {
			printf("Script executed successfully: %s.%s()\n", filename, init_function_name);
		}

		Py_INCREF(pModule);
		return pModule;
	}
}

static int append_pyapm_paths(void) {
	// Import the sys module
    PyObject* sys_module AUTO_DECREF = PyImport_ImportModule("sys");
    if (sys_module == NULL) {
        printf("Failed to import sys module\n");
        return -1;
    }

    // Get the sys.path attribute
    PyObject* sys_path AUTO_DECREF = PyObject_GetAttrString(sys_module, "path");
    if (sys_path == NULL) {
        printf("Failed to get sys.path attribute\n");
        return -2;
    }

    // Append /usr/lib/csh to sys.path
    const char* usr_lib_csh = "/usr/lib/csh";
    PyObject* usr_lib_csh_path AUTO_DECREF = PyUnicode_FromString(usr_lib_csh);
    if (usr_lib_csh_path == NULL) {
        printf("Failed to create Python object for /usr/lib/csh\n");
    } else {
        PyObject* full_path AUTO_DECREF = PyUnicode_FromFormat("%s", usr_lib_csh);
        if (full_path == NULL) {
            printf("Failed to create Python object for /usr/lib/csh\n");
        } else {
			PyList_Insert(sys_path, 0, full_path);
        }
    }

	// Append $HOME to sys.path
    const char* home = getenv("HOME");
    if (home == NULL) {
        printf("HOME environment variable not set\n");
    } else {
        PyObject* home_path AUTO_DECREF = PyUnicode_FromString(home);
        if (home_path == NULL) {
            printf("Failed to create Python object for HOME directory\n");
        } else {
			PyList_Insert(sys_path, 0, home_path);
        }
    }

    // Append $HOME/.local/lib/csh to sys.path
    const char* local_lib_csh = "/.local/lib/csh"; // assuming it's appended to $HOME
    PyObject* local_lib_csh_path AUTO_DECREF = PyUnicode_FromString(local_lib_csh);
    if (local_lib_csh_path == NULL) {
        printf("Failed to create Python object for $HOME/.local/lib/csh\n");
    } else {
        PyObject* full_path AUTO_DECREF = PyUnicode_FromFormat("%s%s", home, local_lib_csh);
        if (full_path == NULL) {
            printf("Failed to create Python object for $HOME/.local/lib/csh\n");
        } else {
			PyList_Insert(sys_path, 0, full_path);
        }
    }

    // Append CWD to sys.path
    PyObject* cwd_path AUTO_DECREF = PyUnicode_FromString(".");
    if (cwd_path == NULL) {
        printf("Failed to create Python object for current working directory\n");
    } else {
        PyList_Append(sys_path, cwd_path);
    }
    // PyObject* pycsh_module AUTO_DECREF = PyImport_ImportModule("pycsh");
    // if (pycsh_module == NULL) {
    //     printf("Failed to import pycsh\n");
    //     return -1;
    // }

    return 0;
}

static void raise_exception_in_all_threads(PyObject *exception) {
    PyGILState_STATE gstate = PyGILState_Ensure();
    PyInterpreterState *interp = PyInterpreterState_Head();
    PyThreadState *tstate = PyInterpreterState_ThreadHead(interp);

    while (tstate != NULL) {
        if (tstate != PyThreadState_Get()) {  // Skip the current thread
            PyThreadState_SetAsyncExc(tstate->thread_id, exception);
        }
        tstate = PyThreadState_Next(tstate);
    }

    PyGILState_Release(gstate);
}

__attribute__((destructor(150))) static void finalize_python_interpreter(void) {
	/* TODO Kevin: Is seems that this deallocator will still be called,
		even if the shared library (PyCSH) isn't loaded correctly.
		Which will cause a segmentation fault when finalizing Python.
		So we should make a guard clause for that. */
	PyThreadState* tstate = PyGILState_GetThisThreadState();
	if(tstate == NULL){
		return;
	}
	PyEval_RestoreThread(main_thread_state);  // Re-acquire the GIL so we can raise and exit \_(ツ)_/¯
	raise_exception_in_all_threads(PyExc_SystemExit);
	/* NOTE: It seems exceptions don't interrupt sleep() in threads,
		so Py_FinalizeEx() still has to wait for them to finish.
		I have tried calling PyEval_SaveThread(); and here PyEval_RestoreThread(main_thread_state); here,
		but that doesn't seem to solve the problem. */
	if (Py_FinalizeEx() < 0) {
		/* We probably don't have to die to unloading errors here,
			but maybe the user wants to know. */
        fprintf(stderr, "Error finalizing Python interpreter\n");
    }
}

void py_init_interpreter(void) {

    if (Py_IsInitialized()) {
        return;
    }

    /* Calling Py_Initialize() "has the side effect of locking the global interpreter lock.
        Once the function completes, you are responsible for releasing the lock."
        -- https://www.linuxjournal.com/article/3641 */
    extern PyMODINIT_FUNC PyInit_pycsh(void);
    PyImport_AppendInittab("pycsh", PyInit_pycsh);
    Py_Initialize();
    if (append_pyapm_paths()) {
        fprintf(stderr, "Failed to add Python APM paths\n");
    }
    // release GIL here
    main_thread_state = PyEval_SaveThread();

	return;
}

static void walk_path_list(char *pathlist, char *search_str, void (*cb)(char *path, char *search_str)) {
	size_t pathlist_len = strlen(pathlist);
	char *runner = pathlist;
	char *start = pathlist;
	const char *end = pathlist + pathlist_len;
	while (runner++ < end) {
		if(*runner == ';') {
			*runner = '\0';
			cb(start, search_str);
			start = ++runner;
		} else if(*runner == '\0') {
			cb(start, search_str);
			start = ++runner;
		}
	}
}

static void load_py(char *path, char *search_str) {
	struct dirent *entry;
	DIR *dir CLEANUP_DIR = opendir(path);
	if (dir == NULL) {
		perror("opendir");
	} else {
		while ((entry = readdir(dir)) != NULL)  {

			/* Verify file has search string */
			if (search_str && !strstr(entry->d_name, search_str)) {
				continue;
			}
			if(strstr(entry->d_name, ".py")) {
				int fullpath_len = strnlen(path, WALKDIR_MAX_PATH_SIZE) + strnlen(entry->d_name, WALKDIR_MAX_PATH_SIZE);
				char fullpath[fullpath_len+1];
				strncpy(fullpath, path, fullpath_len);
				strncat(fullpath, entry->d_name, fullpath_len-strnlen(path, WALKDIR_MAX_PATH_SIZE));
				PyObject * pymod AUTO_DECREF = pycsh_load_pymod(fullpath, DEFAULT_INIT_FUNCTION, 1);
				if (pymod == NULL) {
					PyErr_Print();
					continue;
				}
				apm_entry_t * e = calloc(1, sizeof(apm_entry_t));
				if (!e) {
					printf("Memory allocation error.\n");
				} else {
					e->apm_init_version = APM_INIT_VERSION;
					strncpy(e->path, fullpath, WALKDIR_MAX_PATH_SIZE - 1);
					size_t i = strlen(e->path);
					while ((i > 0) && (e->path[i-1] != '/')) {
						i--;
					}
					e->file = &(e->path[i]);
					// TODO Kevin: Verbose argument?
					printf("\033[32mLoaded: %s\033[0m\n", fullpath);
					apm_queue_add(e);
				}
			}
		}
	}
}

int py_apm_load_cmd(struct slash *slash) {

    char * path = NULL;
    char * search_str = NULL;
	bool free_path = false;

    optparse_t * parser = optparse_new("apm load", "-f <filename> -p <pathname>");
    optparse_add_help(parser);
    optparse_add_string(parser, 'p', "path", "PATHNAME", &path, "Search paths separated by ';'");
    optparse_add_string(parser, 'f', "file", "FILENAME", &search_str, "Search string on APM file name");

    int argi = optparse_parse(parser, slash->argc - 1, (const char **) slash->argv + 1);
    if (argi < 0) {
        optparse_del(parser);
	    return SLASH_EINVAL;
    }

    if (path == NULL) {
        char *home_dir = getenv("HOME");
        if (home_dir == NULL) {
            home_dir = getpwuid(getuid())->pw_dir;
            if (home_dir == NULL) {
                optparse_del(parser);
                printf("No home folder found\n");
                return SLASH_EINVAL;
            }
        }
        path = (char *)malloc(WALKDIR_MAX_PATH_SIZE);
		if(path) {
	        strcpy(path, home_dir);
    	    strcat(path, PYAPMS_DIR);
	        strcat(path, ";/usr/lib/csh/");
			free_path = true;
		}
    }
	if(path) {
		PyEval_RestoreThread(main_thread_state);
		PyThreadState *state __attribute__((cleanup(state_release_GIL))) = main_thread_state;
		if (main_thread_state == NULL) {
			fprintf(stderr, "main_thread_state is NULL\n");
			return SLASH_EINVAL;
		}
		walk_path_list(path, search_str, load_py);
	}
	if(free_path) {
		free(path);
	}
	optparse_del(parser);
	return SLASH_SUCCESS;
}

static wchar_t **handle_py_argv(char **args, int argc) {
	wchar_t **res = calloc(sizeof(wchar_t *), argc);
	if (res) {
		for (int i = 0; i < argc; i++) {
			res[i] = Py_DecodeLocale(args[i], NULL);
		}
	}
	return res;
}

const struct slash_command slash_cmd_python;
static int python_slash(struct slash *slash) {
	int res =  0;
    char * string = NULL;

    optparse_t * parser = optparse_new_ex(slash_cmd_python.name, slash_cmd_python.args, slash_cmd_python.help);
    optparse_add_help(parser);
    optparse_add_string(parser, 'c', "string", "STRING", &string, "Execute Python code in given string (much like the standard Python interpreter)");

    int argi = optparse_parse(parser, slash->argc - 1, (const char **) slash->argv + 1);
    if (argi < 0) {
        optparse_del(parser);
	    return SLASH_EINVAL;
    }

	PyEval_RestoreThread(main_thread_state);
	PyThreadState *state __attribute__((cleanup(state_release_GIL))) = main_thread_state;
	if (main_thread_state == NULL) {
		fprintf(stderr, "main_thread_state is NULL\n");
		optparse_del(parser);
		return SLASH_EINVAL;
	}

	PyRun_SimpleString("import pycsh");

	/* Python 3.11 has deprecated `PySys_SetArgv()` (while keeping it in the stable API),
		3.11 simultaneously introduces `PyConfig.argv` as the intended replacement.
		We do not yet want to require 3.11, so we just silence the warnings instead. */
	#pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wdeprecated-declarations"

	wchar_t **argv = NULL;
	if(string) {
		argv = handle_py_argv(slash->argv + argi, slash->argc - argi);
		PySys_SetArgv(slash->argc - argi, argv);
		res = PyRun_SimpleString(string);
	} else if (++argi < slash->argc) {
		FILE *fp = fopen(slash->argv[argi], "rb");
		if(fp) {
			argv = handle_py_argv(slash->argv + argi, slash->argc - argi);
			PySys_SetArgv(slash->argc - argi, argv);

			{   /* `PyRun_AnyFileEx()` doesn't define `__file__`, so we do it ourselves. */
				PyObject *main_mod = PyImport_AddModule("__main__");
				PyObject *main_dict = PyModule_GetDict(main_mod);
				PyObject *file_str = PyUnicode_FromString(slash->argv[argi]);
				PyDict_SetItemString(main_dict, "__file__", file_str);
				Py_DECREF(file_str);
			}

			res = PyRun_AnyFileEx(fp, slash->argv[argi], 1);

		} else {
			res = SLASH_EINVAL;
		}
	} else {
		slash_release_std_in_out(slash);
		PySys_SetArgv(0, NULL);
		PyRun_SimpleString("import rlcompleter");
		PyRun_SimpleString("import readline");
		PyRun_SimpleString("readline.parse_and_bind(\"tab: complete\")");
		res = PyRun_InteractiveLoop(stdin, "<stdin>");
		slash_acquire_std_in_out(slash);
	}

	#pragma GCC diagnostic pop  /* -Wdeprecated-declarations */

	if(argv) {
		for (int i = 0; i < (slash->argc - argi); i++) {
			PyMem_RawFree(argv[i]);
		}		
		free(argv);
	}
	optparse_del(parser);
	return res;
}

slash_command_completer(python, python_slash, slash_path_completer, "[(-c <python_string>|<filename>) [args...]]", "Starts an interactive Python interpreter in the current CSH process\n"\
	"or execute the script in given file.\n"
	"This allows you to run pretty much any Python code, particularly code using PyCSH which allows for interacting\n"\
	"with CSP nodes.\n\nUse \"Control-D\" to exit the interpreter and return to CSH.");
