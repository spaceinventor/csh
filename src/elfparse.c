#include <libelf.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <gelf.h>

#include <slash/slash.h>
#include <slash/optparse.h>

#include <param/param.h>
#include <csp/csp.h>

Elf_Scn * elfparse_section_from_vma(Elf * elf, intptr_t addr) {
    Elf_Scn *section = NULL;
	while ((section = elf_nextscn(elf, section)) != 0) {
		GElf_Shdr shdr, *sh;
		sh = gelf_getshdr(section, &shdr);
        if ((addr >= sh->sh_addr) && (addr < sh->sh_addr + sh->sh_size)) {
            printf("section for %lx at: addr %lx, size %lu, offset %lx, type %x\n", addr, sh->sh_addr, sh->sh_size, sh->sh_offset, sh->sh_type);
            return section;
        }
	}
    return NULL;
}

char * elfparse_str_from_vma(Elf * elf, intptr_t addr) {

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

    printf("Data %p %lx %lu %u\n", data->d_buf, data->d_off, data->d_size, data->d_type);

    GElf_Shdr shdr, *sh;
    sh = gelf_getshdr(section, &shdr);

    printf("Offset section %lx\n", sh->sh_addr);
    printf("Addr %lx\n", addr);
    char * ramaddr = data->d_buf + addr - sh->sh_addr;
    printf("ramaddr %p\n", ramaddr);
    printf("string %s\n", ramaddr);
    return ramaddr;

}


void elfparse_native(Elf * elf, char * buf, unsigned long offset, intptr_t start, intptr_t stop, int verbose) {

    /* Packing formats are different on different platforms */
    int param_size = sizeof(param_t);
    int param_section_size = stop - start;
    int param_count = param_section_size / param_size;

    if (verbose > 2) {
        printf("Sizeof param_t %u b\n", param_size);
        printf("Param size %d b\n", param_section_size);
        printf("Total found %d\n", param_count);
    }

    param_t * first_param = (param_t *) &buf[start - offset];
    param_t * last_param = (param_t *) &buf[stop - offset];
    param_t * param = first_param;
    printf("buf %p %lu\n", buf, offset);
    while(param < last_param) {
        printf("param id %hu, node %hu, type %u, name_ptr %p\n", param->id, param->node, param->type, param->name);

        char * name = elfparse_str_from_vma(elf, (intptr_t) param->name);
        printf("name %s\n", name);
        param++;
    }

}

void elfparse_32arm(Elf * elf, char * buf, unsigned long offset, intptr_t start, intptr_t stop, int verbose) {

    typedef struct {
        uint16_t id;
        uint16_t node;
        uint32_t type;
        uint32_t mask;
        uint32_t name_ptr;
        uint32_t unit_ptr;
        uint32_t docstr_ptr;
        uint32_t addr_ptr;
        uint32_t vmem_ptr;
        uint32_t array_size;
        uint32_t array_step;
        uint32_t callback_ptr;
        uint32_t timestamp;
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

    param_format_1 * first_param = (param_format_1 *) &buf[start - offset];
    param_format_1 * last_param = (param_format_1 *) &buf[stop - offset];
    param_format_1 * param = first_param;

    while(param < last_param) {
        char * name = &buf[param->name_ptr - offset];
        printf("param id %hu, node %hu, type %u, name_ptr %u, name %s\n", param->id, param->node, param->type, param->name_ptr, name);
        param++;
    }

}

static int elfparse(struct slash * slash) {

    int verbose = 4;
    int param_parser = 0;

	optparse_t * parser = optparse_new("elf param", "<file>");
	optparse_add_help(parser);
    optparse_add_int(parser, 'v', "verbose", "LEVEL", 10, &verbose, "1 = normal, 2 = extra");
    optparse_add_int(parser, 'p', "parser", "TYPE", 10, &param_parser, "0 = native, 1 = 32-bit (default)");

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

    GElf_Ehdr header;
	if (!gelf_getehdr(elf, &header))
		goto out_elf;
    
    /* Search for __start_param and __stop_param symbols */
    Elf64_Addr start_param = 0;
    Elf64_Addr stop_param = 0;
    
	Elf_Scn *section = NULL;
	while ((section = elf_nextscn(elf, section)) != 0) {
		GElf_Shdr shdr, *sh;
		sh = gelf_getshdr(section, &shdr);
        char *secname = elf_strptr(elf, header.e_shstrndx, sh->sh_name);

        if (verbose > 0) {
            printf("section %s: addr %lx, size %lu, offset %lx, type %x\n", secname, sh->sh_addr, sh->sh_size, sh->sh_offset, sh->sh_type);
        }

		if (sh->sh_type == SHT_SYMTAB || sh->sh_type == SHT_DYNSYM) {
			Elf_Data *data = elf_getdata(section, NULL);
			GElf_Sym *sym, symbol;
			int j;

			unsigned numsym = sh->sh_size / sh->sh_entsize;
			for (j = 0; j < numsym; j++) {
				sym = gelf_getsymshndx(data, NULL, j, &symbol, NULL);
				char *symname = elf_strptr(elf, shdr.sh_link, sym->st_name);
                if (strcmp(symname, "__start_param") == 0) {
                    start_param = sym->st_value;
                    if (verbose > 1) {
                        printf("Found __start_param at 0x%lx\n", start_param);
                    }
                }
                if (strcmp(symname, "__stop_param") == 0) {
			        stop_param = sym->st_value;
                    if (verbose > 1) {
                        printf("Found __stop_param at 0x%lx\n", stop_param);
                    }
                }
			}
		}
	}

    if (!start_param || !stop_param) {
        printf("Unable to find __start_param, __stop_param and .text section\n");
        goto out_elf;
    }

    /* Okay, we got the section id, the pointers now get the data */
    section = elfparse_section_from_vma(elf, start_param);
    Elf_Data * data = elf_getdata(section, NULL);

    if (data == NULL) {
        printf("Unable to read .text section\n");
        goto out_elf;
    }

    printf("Data %p %lx %lu %u\n", data->d_buf, data->d_off, data->d_size, data->d_type);

    if (data->d_buf == NULL) {
        printf("Invalid text section pointer\n");
        goto out_elf;
    }

    GElf_Shdr shdr, *sh;
    sh = gelf_getshdr(section, &shdr);

    printf("Text in section %lx\n", sh->sh_addr);

    if (sh == NULL) {
        printf("Could not get .text header\n");
        goto out_elf;
    }

    switch(param_parser) {
        case 0:
            elfparse_native(elf, data->d_buf, sh->sh_addr, start_param, stop_param, verbose);
        break;
        default:
            elfparse_32arm(elf, data->d_buf, sh->sh_addr, start_param, stop_param, verbose);
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
