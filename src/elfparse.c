#include <libelf.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <gelf.h>

#include <slash/slash.h>
#include <slash/optparse.h>

#include <param/param.h>
#include <param/param_string.h>
#include <csp/csp.h>

Elf_Scn * elfparse_section_from_vma(Elf * elf, intptr_t addr) {
    Elf_Scn *section = NULL;
	while ((section = elf_nextscn(elf, section)) != 0) {
		GElf_Shdr shdr, *sh;
		sh = gelf_getshdr(section, &shdr);
        if ((addr >= sh->sh_addr) && (addr < sh->sh_addr + sh->sh_size)) {
            //printf("section for %lx at: addr %lx, size %lu, offset %lx, type %x\n", addr, sh->sh_addr, sh->sh_size, sh->sh_offset, sh->sh_type);
            return section;
        }
	}
    return NULL;
}

char * elfparse_ptr_from_vma(Elf * elf, intptr_t addr) {

    if (addr == 0)
        return NULL;

    Elf_Scn * section = elfparse_section_from_vma(elf, addr);

    if (section == NULL) {
        printf("Unable to locate section for %lx\n", addr);
        return NULL;
    }

    Elf_Data * data = elf_getdata(section, NULL);
    if (data == NULL) {
        printf("Unable to read data\n");
        return NULL;
    }

    GElf_Shdr shdr, *sh;
    sh = gelf_getshdr(section, &shdr);

    char * ramaddr = data->d_buf + addr - sh->sh_addr;

    //printf("Data %p %lx %lu %u\n", data->d_buf, data->d_off, data->d_size, data->d_type);
    //printf("Offset section %lx\n", sh->sh_addr);
    //printf("Addr %lx\n", addr);
    //printf("ramaddr %p\n", ramaddr);
    //printf("string %s\n", ramaddr);
    
    return ramaddr;

}

intptr_t elfparse_addr_from_symbol(Elf * elf, char * symbol_name) {

    //GElf_Ehdr header;
	//if (!gelf_getehdr(elf, &header))
	//	return 0;
    
	Elf_Scn *section = NULL;
	while ((section = elf_nextscn(elf, section)) != 0) {
		GElf_Shdr shdr, *sh;
		sh = gelf_getshdr(section, &shdr);
        //char *secname = elf_strptr(elf, header.e_shstrndx, sh->sh_name);
        //printf("section %s: addr %lx, size %lu, offset %lx, type %x\n", secname, sh->sh_addr, sh->sh_size, sh->sh_offset, sh->sh_type);

		if (sh->sh_type == SHT_SYMTAB || sh->sh_type == SHT_DYNSYM) {
			Elf_Data *data = elf_getdata(section, NULL);
			GElf_Sym *sym, symbol;
			int j;

			unsigned numsym = sh->sh_size / sh->sh_entsize;
			for (j = 0; j < numsym; j++) {
				sym = gelf_getsymshndx(data, NULL, j, &symbol, NULL);
				char *symname = elf_strptr(elf, shdr.sh_link, sym->st_name);
                if (strcmp(symname, symbol_name) == 0) {
                    return sym->st_value;
                }
			}
		}
	}

    return 0;

}


void elfparse_native(Elf * elf, intptr_t start, intptr_t stop, int verbose) {

    /* Packing formats are different on different platforms */
    int param_size = sizeof(param_t);
    int param_section_size = stop - start;
    int param_count = param_section_size / param_size;

    if (verbose > 2) {
        printf("Sizeof param_t %u b\n", param_size);
        printf("Param size %d b\n", param_section_size);
        printf("Total found %d\n", param_count);
    }

    param_t * first_param = (param_t *) elfparse_ptr_from_vma(elf, start);
    param_t * last_param = first_param + param_count;
    param_t * param = first_param;
    
    while(param < last_param) {
        char * name = elfparse_ptr_from_vma(elf, (intptr_t) param->name);
        char * docstr = elfparse_ptr_from_vma(elf, (intptr_t) param->docstr);
        char * unit = elfparse_ptr_from_vma(elf, (intptr_t) param->unit);
        //printf("param id %hu, node %hu, type %u, name_ptr %p, name %s, unit %s, docstr %s\n", param->id, param->node, param->type, param->name, name, unit, docstr);

        printf( "list add ");

        if (param->array_size > 1) {
            printf("-a %u ", param->array_size);
        }
        if ((docstr != NULL) && (strlen(docstr) > 0)) {
            printf("-c \"%s\" ", docstr);
        }
        if ((unit != NULL) && (strlen(unit) > 0)) {
            printf("-u \"%s\"", unit);
        }
        
		if (param->mask > 0) {
			unsigned int mask = param->mask;

			printf( "-m \"");

			if (mask & PM_READONLY) {
				mask &= ~ PM_READONLY;
				printf("r");
			}

			if (mask & PM_REMOTE) {
				mask &= ~ PM_REMOTE;
				printf("R");
			}

			if (mask & PM_CONF) {
				mask &= ~ PM_CONF;
				printf("c");
			}

			if (mask & PM_TELEM) {
				mask &= ~ PM_TELEM;
				printf("t");
			}

			if (mask & PM_HWREG) {
				mask &= ~ PM_HWREG;
				printf("h");
			}

			if (mask & PM_ERRCNT) {
				mask &= ~ PM_ERRCNT;
				printf("e");
			}

			if (mask & PM_SYSINFO) {
				mask &= ~ PM_SYSINFO;
				printf("i");
			}

			if (mask & PM_SYSCONF) {
				mask &= ~ PM_SYSCONF;
				printf("C");
			}

			if (mask & PM_WDT) {
				mask &= ~ PM_WDT;
				printf("w");
			}

			if (mask & PM_DEBUG) {
				mask &= ~ PM_DEBUG;
				printf("d");
			}

			if (mask & PM_ATOMIC_WRITE) {
				mask &= ~ PM_ATOMIC_WRITE;
				printf("o");
			}

			if (mask & PM_CALIB) {
				mask &= ~ PM_CALIB;
				printf("q");
			}

			//if (mask)
			//	fprintf(out, "+%x", mask);

            printf("\" ");

		}
		
        printf("%s %u ", name, param->id);

        char typestr[10];
        param_type_str(param->type, typestr, 10);
        printf("%s\n", typestr);

        param++;
    }

}

void elfparse_32arm(Elf * elf, intptr_t start, intptr_t stop, int verbose, int list_dynamic) {

	typedef struct {
        uint16_t id;
        uint16_t node;
        uint8_t type;  // Type appears to be packed to 1 byte on 32-bit ARM
        uint32_t mask;
        uint32_t name;
        uint32_t unit;
        uint32_t docstr;
        uint32_t addr;
        uint32_t vmem;
        int32_t array_size;
        int32_t array_step;
        uint32_t callback_ptr;
        uint32_t timestamp;
		// uint32_t next;  /* Manually handle next pointer, based on arguments provided. */
    } param_format_1;

    /* Packing formats are different on different platforms */
    int param_size = sizeof(param_format_1);
    int param_section_size = stop - start;
    int param_count = param_section_size / param_size;

    if (verbose > 2) {
        printf("Sizeof param_t %u b\n", param_size);
        printf("Param size %d b\n", param_section_size);
        printf("Total found %d\n", param_count);
    }

    param_format_1 * first_param = (param_format_1 *) elfparse_ptr_from_vma(elf, start);
    param_format_1 * last_param = first_param + param_count;
    param_format_1 * param = first_param;

    while(param < last_param) {
        char * name = elfparse_ptr_from_vma(elf, (intptr_t) param->name);
        char * docstr = elfparse_ptr_from_vma(elf, (intptr_t) param->docstr);
        char * unit = elfparse_ptr_from_vma(elf, (intptr_t) param->unit);
        //printf("param id %hu, node %hu, type %u, name_ptr %p, name %s, unit %s, docstr %s\n", param->id, param->node, param->type, param->name, name, unit, docstr);

        printf( "list add ");

        if (param->array_size > 1) {
            printf("-a %u ", param->array_size);
        }
        if ((docstr != NULL) && (strlen(docstr) > 0)) {
            printf("-c \"%s\" ", docstr);
        }
        if ((unit != NULL) && (strlen(unit) > 0)) {
            printf("-u \"%s\"", unit);
        }
        
		if (param->mask > 0) {
			unsigned int mask = param->mask;

			printf( "-m \"");

			if (mask & PM_READONLY) {
				mask &= ~ PM_READONLY;
				printf("r");
			}

			if (mask & PM_REMOTE) {
				mask &= ~ PM_REMOTE;
				printf("R");
			}

			if (mask & PM_CONF) {
				mask &= ~ PM_CONF;
				printf("c");
			}

			if (mask & PM_TELEM) {
				mask &= ~ PM_TELEM;
				printf("t");
			}

			if (mask & PM_HWREG) {
				mask &= ~ PM_HWREG;
				printf("h");
			}

			if (mask & PM_ERRCNT) {
				mask &= ~ PM_ERRCNT;
				printf("e");
			}

			if (mask & PM_SYSINFO) {
				mask &= ~ PM_SYSINFO;
				printf("i");
			}

			if (mask & PM_SYSCONF) {
				mask &= ~ PM_SYSCONF;
				printf("C");
			}

			if (mask & PM_WDT) {
				mask &= ~ PM_WDT;
				printf("w");
			}

			if (mask & PM_DEBUG) {
				mask &= ~ PM_DEBUG;
				printf("d");
			}

			if (mask & PM_ATOMIC_WRITE) {
				mask &= ~ PM_ATOMIC_WRITE;
				printf("o");
			}

			if (mask & PM_CALIB) {
				mask &= ~ PM_CALIB;
				printf("q");
			}

			//if (mask)
			//	fprintf(out, "+%x", mask);

            printf("\" ");

		}
		
        printf("%s %u ", name, param->id);

        char typestr[10];
        param_type_str(param->type, typestr, 10);
        printf("%s\n", typestr);

		if (!list_dynamic) {  /* assumes param_format_1 is without list_dynamic ie 'next' */
        	param++;
		} else {  /* Manually increment past the 'next' pointer */
			/* 4 bytes is the size of the 'next' pointer */
			param = (param_format_1*)((void*)param + 4 + sizeof(param_format_1));
		}
    }

}

static int elfparse(struct slash * slash) {

    int verbose = 4;
    int param_parser = 1;

	optparse_t * parser = optparse_new("elf param", "<file>");
	optparse_add_help(parser);
    optparse_add_int(parser, 'v', "verbose", "LEVEL", 10, &verbose, "1 = normal, 2 = extra");
    optparse_add_int(parser, 'p', "parser", "TYPE", 10, &param_parser, "0 = native, 1 = 32-bit (default), 2 = 32-bit + dyn");

	int argi = optparse_parse(parser, slash->argc - 1, (const char **)slash->argv + 1);
	if (argi < 0) {
		optparse_del(parser);
		return SLASH_EINVAL;
	}

	/* Expect filename */
	if (++argi >= slash->argc) {
		printf("missing filename\n");
		return SLASH_EINVAL;
	}

	char * filename = slash->argv[argi];

	printf("Readelf %s\n", filename);

	elf_version(EV_CURRENT);
	int fd = open(filename, O_RDONLY);
	if (fd < 0)
		return SLASH_EINVAL;

	Elf *elf = elf_begin(fd, ELF_C_READ, NULL);
	if (elf == NULL)
		goto out;

    
    /* Search for __start_param and __stop_param symbols */
    Elf64_Addr start_param = elfparse_addr_from_symbol(elf, "__start_param");
    Elf64_Addr stop_param = elfparse_addr_from_symbol(elf, "__stop_param");

    if (!start_param || !stop_param) {
        printf("Unable to find __start_param, __stop_param\n");
        goto out_elf;
    }

    switch(param_parser) {
        case 0:
            elfparse_native(elf, start_param, stop_param, verbose);
        break;
        default:
            elfparse_32arm(elf, start_param, stop_param, verbose, (param_parser == 2));
            break;
    }

    return SLASH_SUCCESS;

out_elf:
	elf_end(elf);

out: 
    printf("Error\n");
	close(fd);
	return SLASH_EINVAL;
}

slash_command(elfparse, elfparse, NULL, NULL);
