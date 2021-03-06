
/*****************************************************************************\
 *  slurm_topology.c - Topology plugin function setup.
 *****************************************************************************
 *  Copyright (C) 2009-2010 Lawrence Livermore National Security.
 *  Produced at Lawrence Livermore National Laboratory (cf, DISCLAIMER).
 *  Written by Morris Jette <jette1@llnl.gov>
 *  CODE-OCEC-09-009. All rights reserved.
 *
 *  This file is part of SLURM, a resource management program.
 *  For details, see <http://www.schedmd.com/slurmdocs/>.
 *  Please also read the included file: DISCLAIMER.
 *
 *  SLURM is free software; you can redistribute it and/or modify it under
 *  the terms of the GNU General Public License as published by the Free
 *  Software Foundation; either version 2 of the License, or (at your option)
 *  any later version.
 *
 *  In addition, as a special exception, the copyright holders give permission
 *  to link the code of portions of this program with the OpenSSL library under
 *  certain conditions as described in each individual source file, and
 *  distribute linked combinations including the two. You must obey the GNU
 *  General Public License in all respects for all of the code used other than
 *  OpenSSL. If you modify file(s) with this exception, you may extend this
 *  exception to your version of the file(s), but you are not obligated to do
 *  so. If you do not wish to do so, delete this exception statement from your
 *  version.  If you delete this exception statement from all source files in
 *  the program, then also delete it here.
 *
 *  SLURM is distributed in the hope that it will be useful, but WITHOUT ANY
 *  WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 *  FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 *  details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with SLURM; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301  USA.
\*****************************************************************************/

#include <pthread.h>

#include "src/common/log.h"
#include "src/common/plugrack.h"
#include "src/common/slurm_protocol_api.h"
#include "src/common/slurm_topology.h"
#include "src/common/timers.h"
#include "src/common/xmalloc.h"
#include "src/common/xstring.h"

/* defined here but is really tree plugin related */
struct switch_record *switch_record_table = NULL;
int switch_record_cnt = 0;

/* ************************************************************************ */
/*  TAG(                        slurm_topo_ops_t                         )  */
/* ************************************************************************ */
typedef struct slurm_topo_ops {
	int		(*build_config)		( void );
	bool		(*node_ranking)		( void );
	int		(*get_node_addr)	( char* node_name,
						  char** addr,
						  char** pattern );
} slurm_topo_ops_t;

/*
 * Must be synchronized with slurm_topo_ops_t above.
 */
static const char *syms[] = {
	"topo_build_config",
	"topo_generate_node_ranking",
	"topo_get_node_addr",
};

static slurm_topo_ops_t ops;
static plugin_context_t	*g_context = NULL;
static pthread_mutex_t g_context_lock = PTHREAD_MUTEX_INITIALIZER;
static bool init_run = false;

/* *********************************************************************** */
/*  TAG(                        slurm_topo_init                         )  */
/*                                                                         */
/*  NOTE: The topology plugin can not be changed via reconfiguration       */
/*        due to background threads, job priorities, etc. Slurmctld must   */
/*        be restarted  and job priority changes may be required to change */
/*        the topology type.                                               */
/* *********************************************************************** */
extern int
slurm_topo_init( void )
{
	int retval = SLURM_SUCCESS;
	char *plugin_type = "topo";
	char *type = NULL;

	if (init_run && g_context)
		return retval;

	slurm_mutex_lock(&g_context_lock);

	if (g_context)
		goto done;

	type = slurm_get_topology_plugin();

	g_context = plugin_context_create(
		plugin_type, type, (void **)&ops, syms, sizeof(syms));

	if (!g_context) {
		error("cannot create %s context for %s", plugin_type, type);
		retval = SLURM_ERROR;
		goto done;
	}
	init_run = true;

done:
	slurm_mutex_unlock(&g_context_lock);
	xfree(type);
	return retval;
}

/* *********************************************************************** */
/*  TAG(                        slurm_topo_fini                         )  */
/* *********************************************************************** */
extern int
slurm_topo_fini( void )
{
	int rc;

	if (!g_context)
		return SLURM_SUCCESS;

	init_run = false;
	rc = plugin_context_destroy(g_context);
	g_context = NULL;
	return rc;
}


/* *********************************************************************** */
/*  TAG(                      slurm_topo_build_config                   )  */
/* *********************************************************************** */
extern int
slurm_topo_build_config( void )
{
	int rc;
	DEF_TIMERS;

	if ( slurm_topo_init() < 0 )
		return SLURM_ERROR;

	START_TIMER;
	rc = (*(ops.build_config))();
	END_TIMER3("slurm_topo_build_config", 20000);

	return rc;
}

/* *********************************************************************** */
/*  TAG(                      slurm_topo_generate_node_ranking          )  */
/* NOTE: This operation is only supported by those topology plugins for    */
/*       which the node ordering between slurmd and slurmctld is invariant */
/* *********************************************************************** */
extern bool
slurm_topo_generate_node_ranking( void )
{
	if ( slurm_topo_init() < 0 )
		return SLURM_ERROR;

	return (*(ops.node_ranking))();
}

/* *********************************************************************** */
/*  TAG(                      slurm_topo_get_node_addr                  )  */
/* *********************************************************************** */
extern int
slurm_topo_get_node_addr( char* node_name, char ** addr, char** pattern )
{
	if ( slurm_topo_init() < 0 )
		return SLURM_ERROR;

	return (*(ops.get_node_addr))(node_name,addr,pattern);
}

