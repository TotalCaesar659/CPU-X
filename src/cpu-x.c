/****************************************************************************
*    Copyright © 2014 Xorg
*
*    This program is free software: you can redistribute it and/or modify
*    it under the terms of the GNU General Public License as published by
*    the Free Software Foundation, either version 3 of the License, or
*    (at your option) any later version.
*
*    This program is distributed in the hope that it will be useful,
*    but WITHOUT ANY WARRANTY; without even the implied warranty of
*    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*    GNU General Public License for more details.
*
*    You should have received a copy of the GNU General Public License
*    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*
****************************************************************************/

/*
* cpu-x.c
*/

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <gtk/gtk.h>
#include <glib.h>
#include "cpu-x.h"

#ifdef LIBCPUID
# include <libcpuid/libcpuid.h>
#endif

#ifdef EMBED
# include "../embed/cpu-x.ui.h"
#endif


int main(int argc, char *argv[]) {
	setenv("LC_ALL", "C", 1);
	GtkBuilder *builder;
	Gwid cpu;
	Libcpuid data;
	Lscpu info;
	Dmi extrainfo;

	/* Populate structures */
	empty_labels(&data, &info, &extrainfo);
#ifdef LIBCPUID
	if(libcpuid(&data))
		fprintf(stderr, "%s: %s: %i: fails in call 'libcpuid(&data)'.\n", PRGNAME, __FILE__, __LINE__);
#endif
	if(ext_lscpu(&info))
		fprintf(stderr, "%s: %s: %i: fails in call 'ext_lscpu(&info))'.\n", PRGNAME, __FILE__, __LINE__);
	if(!getuid()) {
		if(ext_dmidecode(&extrainfo))
			fprintf(stderr, "%s: %s: %i: fails in call 'ext_dmidecode(&extrainfo)'.\n", PRGNAME, __FILE__, __LINE__);
	}

	/* Build UI from Glade file */
	gtk_init(&argc, &argv);
	builder = gtk_builder_new();
#ifdef EMBED
	if(!gtk_builder_add_from_string(builder, cpux_glade, -1, NULL)) {
		g_printerr("%s (error in file %s at line %i) : gtk_builder_add_from_string failed.\n", PRGNAME, __FILE__, __LINE__);
		exit(EXIT_FAILURE);
	}
#else
	if(!gtk_builder_add_from_file(builder, DATADIR("cpu-x.ui"), NULL)) {
		g_printerr("%s (error in file %s at line %i) : gtk_builder_add_from_file failed.\n", PRGNAME, __FILE__, __LINE__);
		exit(EXIT_FAILURE);
	}
#endif
	g_set_prgname(PRGNAME);
	cpu.window	= GTK_WIDGET(gtk_builder_get_object(builder, "window"));
	cpu.okbutton	= GTK_WIDGET(gtk_builder_get_object(builder, "okbutton"));
	cpu.lprgver	= GTK_WIDGET(gtk_builder_get_object(builder, "lprgver"));
	cpu.notebook1	= GTK_WIDGET(gtk_builder_get_object(builder, "notebook1"));
	build_tab_cpu(builder, &cpu);
	g_object_unref(G_OBJECT(builder));
	set_colors(&cpu);
	set_vendorlogo(&cpu, &data);
	set_labels(&cpu, &data, &info, &extrainfo);
	
	g_signal_connect(cpu.window, "destroy", G_CALLBACK(gtk_main_quit), NULL);
	g_signal_connect(cpu.okbutton, "clicked", G_CALLBACK(gtk_main_quit), NULL);

	cpu.threfresh = g_thread_new(NULL, (gpointer)boucle, &cpu);
	gtk_main();

	return EXIT_SUCCESS;
}

int cmd_char(char *command, char *regex, char data[S]) {
	int fd, err = 0, i = 0;
	char c, tmpfile[18] = "/tmp/cpu-x.XXXXXX";
	FILE *extfile = NULL;

	fd = mkstemp(tmpfile);

	err = system( g_strjoin(NULL, command, " | grep ", regex, " | cut -d: -f2", " > ", tmpfile, NULL) );
	if(err != 0)
		fprintf(stderr, "%s: %s: %i: '%s' exited with non-zero.\n", PRGNAME, __FILE__, __LINE__, command);
	
	extfile = fopen(tmpfile, "r");
	if(extfile == NULL) {
		fprintf(stderr, "%s: %s: %i: failed to open file '%s'.\n", PRGNAME, __FILE__, __LINE__, tmpfile);
		perror(tmpfile);
		err++;
	}
	else {
		while(!feof(extfile)) {
			c = fgetc(extfile);
			if(isalnum(c) || c == '_' || c == '-') {
				data[i] = c;
				i++;
			}
		}
		while(i < S) {
			data[i] = '\0';
			i++;
		}

		err = fclose(extfile);
		if(err != 0)
			fprintf(stderr, "%s: %s: %i: failed to close file '%s'.", PRGNAME, __FILE__, __LINE__, command);
		close(fd);
		remove(tmpfile);
	}
	return err;
}

/* Elements provided by libcpuid library */
#ifdef LIBCPUID
int libcpuid(Libcpuid *data) {
	int err = 0;
	struct cpu_raw_data_t raw;
	struct cpu_id_t datanr;

	err += cpuid_get_raw_data(&raw);
	err += cpu_identify(&raw, &datanr);

	sprintf(data->vendor,	"%s",		datanr.vendor_str);
	sprintf(data->name,	"%s",		datanr.cpu_codename);
	sprintf(data->spec,	"%s",		datanr.brand_str);
	sprintf(data->fam,	"%d",		datanr.family);
	sprintf(data->mod,	"%d",		datanr.model);
	sprintf(data->step,	"%d",		datanr.stepping);
	sprintf(data->extfam,	"%d",		datanr.ext_family);
	sprintf(data->extmod,	"%d",		datanr.ext_model);
	sprintf(data->instr,	"%s",		datanr.flags);
	sprintf(data->l1d,	"%d x %d KB",	datanr.num_cores, datanr.l1_data_cache);
	sprintf(data->l1i,	"%d x %d KB",	datanr.num_cores, datanr.l1_instruction_cache);
	sprintf(data->l2,	"%d x %d KB",	datanr.num_cores, datanr.l2_cache);
	sprintf(data->l3,	"%d KB",	datanr.l3_cache);
	sprintf(data->l1dw,	"%d-way",	datanr.l1_assoc);
	sprintf(data->l1iw,	"%d-way",	datanr.l1_assoc);
	sprintf(data->l2w,	"%d-way",	datanr.l2_assoc);
	sprintf(data->l3w,	"%d-way",	datanr.l3_assoc);
	sprintf(data->soc,	"%d",		datanr.total_logical_cpus / datanr.num_cores);
	sprintf(data->core,	"%d",		datanr.num_cores);
	sprintf(data->thrd,	"%d",		datanr.num_logical_cpus);

	return err;
}
#endif

/* Elements provided by command 'lspcu' */
int ext_lscpu(Lscpu *info) {
	int err = 0;

	err += cmd_char("lscpu", "Architecture", info->arch);
	err += cmd_char("lscpu", "'Byte Order'", info->endian);
	err += cmd_char("lscpu", "'CPU MHz'", info->mhz);
	err += cmd_char("lscpu", "'CPU min MHz'", info->mhzmin);
	err += cmd_char("lscpu", "'CPU max MHz'", info->mhzmax);
	err += cmd_char("lscpu", "BogoMIPS", info->mips);
	err += cmd_char("lscpu", "Virtualization", info->virtu);
	sprintf(info->mhz, "%c%c%c%c.%c%cMHz", info->mhz[0],info->mhz[1],info->mhz[2],info->mhz[3],info->mhz[4],info->mhz[5]);

	return err;
}

/* Elements provided by command 'dmidecode' (need root privileges) */
int ext_dmidecode(Dmi *info) {
	int err = 0;

	err += cmd_char("dmidecode -t processor", "'Socket Designation'", info->socket);
	err += cmd_char("dmidecode -t processor", "'External Clock'", info->bus);
	err += cmd_char("dmidecode -t baseboard", "Manufacturer", info->manu);
	err += cmd_char("dmidecode -t baseboard", "'Product Name'", info->model);
	err += cmd_char("dmidecode -t baseboard", "Version", info->rev);
	err += cmd_char("dmidecode -t bios", "Vendor", info->brand);
	err += cmd_char("dmidecode -t bios", "Version", info->version);
	err += cmd_char("dmidecode -t bios", "'Release Date'", info->date);
	err += cmd_char("dmidecode -t bios", "'ROM Size'", info->rom);

	return err;
}

/* Determine CPU multiplicator from base clock */
void mult(char *busfreq, char *cpufreq, char *multmin, char *multmax, char multsynt[Q]) {
	int i, fcpu, fbus, fmin, fmax;
	char ncpu[P] = "", nbus[P] = "";

	for(i = 0; isdigit(cpufreq[i]); i++)
		ncpu[i] = cpufreq[i];
	fcpu = atoi(ncpu);

	for(i = 0; isdigit(cpufreq[i]); i++)
		nbus[i] = busfreq[i];
	nbus[i - 1] = '\0';
	fbus = atoi(nbus);
	fmin = atoi(multmin);
	fmax = atoi(multmax);

	if(fbus > 0)
		sprintf(multsynt, "x %i (%i-%i)", (fcpu / fbus), (fmin / (fbus * 10000)), (fmax / (fbus * 10000)));
}

/* Show some instructions supported by CPU */
void instructions(Lscpu *info, Libcpuid *data, char instr[S]) {
	int sse = 0;
	char found[S] = "";

	if(strstr(data->instr, "mmx") == 0)
		strcat(found, ", MMX");

	if(strstr(data->instr, "sse ") == 0) {
		strcat(found, ", SSE (1");
		sse = 1;
	}
	if(strstr(data->instr, "sse2") == 0)
		strcat(found, ", 2");
	if(strstr(data->instr, "sse3") == 0)
		strcat(found, ", 3");
	if(strstr(data->instr, "sse4_1") == 0)
		strcat(found, ", 4.1");
	if(strstr(data->instr, "sse4_2") == 0)
		strcat(found, ", 4.2");
	if(sse)
		strcat(found, ")");

	sprintf(instr, "%s, %s%s", info->endian, info->virtu, found);
}