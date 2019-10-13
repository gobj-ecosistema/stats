/****************************************************************************
 *          STATS_LIST.C
 *
 *          List stats
 *
 *          Copyright (c) 2018 Niyamaka.
 *          All Rights Reserved.
 ****************************************************************************/
#include <stdio.h>
#include <argp.h>
#include <time.h>
#include <errno.h>
#include <regex.h>
#include <locale.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <ghelpers.h>

/***************************************************************************
 *              Constants
 ***************************************************************************/
#define NAME        "stats_list"
#define DOC         "List statistics."

#define VERSION     __ghelpers_version__
#define SUPPORT     "<niyamaka at yuneta.io>"
#define DATETIME    __DATE__ " " __TIME__

/***************************************************************************
 *              Structures
 ***************************************************************************/
typedef struct {
    char path[256];
    char group[256];
    json_t *match_cond;
    int verbose;
} list_params_t;


/*
 *  Used by main to communicate with parse_opt.
 */
#define MIN_ARGS 0
#define MAX_ARGS 0
struct arguments
{
    char *args[MAX_ARGS+1];     /* positional args */

    char *path;
    char *group;
    int recursive;
    int verbose;
    int limits;

    char *from_t;
    char *to_t;

    char *variable;
    char *metric;
    char *units;
};

/***************************************************************************
 *              Prototypes
 ***************************************************************************/
static error_t parse_opt (int key, char *arg, struct argp_state *state);

/***************************************************************************
 *      Data
 ***************************************************************************/
struct arguments arguments;
int total_counter = 0;
int partial_counter = 0;
const char *argp_program_version = NAME " " VERSION;
const char *argp_program_bug_address = SUPPORT;

/* Program documentation. */
static char doc[] = DOC;

/* A description of the arguments we accept. */
static char args_doc[] = "";

/*
 *  The options we understand.
 *  See https://www.gnu.org/software/libc/manual/html_node/Argp-Option-Vectors.html
 */
static struct argp_option options[] = {
/*-name-----------------key-----arg-----------------flags---doc-----------------group */
{0,                     0,      0,                  0,      "Database",         2},
{"path",                'a',    "PATH",             0,      "Path.",            2},
{"group",               'b',    "GROUP",            0,      "Group.",           2},
{"recursive",           'r',    0,                  0,      "List recursively.",  2},

{0,                     0,      0,                  0,      "Presentation",     3},
{"verbose",             'l',    "LEVEL",            0,      "Verbose level ()", 3},

{0,                     0,      0,                  0,      "Search conditions", 4},
{"from-t",              1,      "TIME",             0,      "From time.",       4},
{"to-t",                2,      "TIME",             0,      "To time.",         4},

{"limits",              3,      0,                  0,      "Show limits",      5},

{"variable",            21,     "VARIABLE",         0,      "Variable.",        9},
{"metric",              22,     "METRIC",           0,      "Metric.",          9},
{"units",               23,     "UNITS",            0,      "Units (SEC, MIN, HOUR, MDAY, MON, YEAR, WDAY, YDAY, CENT).",          9},

{0}
};

/* Our argp parser. */
static struct argp argp = {
    options,
    parse_opt,
    args_doc,
    doc
};

/***************************************************************************
 *  Parse a single option
 ***************************************************************************/
static error_t parse_opt (int key, char *arg, struct argp_state *state)
{
    /*
     *  Get the input argument from argp_parse,
     *  which we know is a pointer to our arguments structure.
     */
    struct arguments *arguments = state->input;

    switch (key) {
    case 'a':
        arguments->path= arg;
        break;
        break;
    case 'b':
        arguments->group= arg;
        break;
    case 'r':
        arguments->recursive = 1;
        break;
    case 'l':
        if(arg) {
            arguments->verbose = atoi(arg);
        }
        break;

    case 1: // from_t
        arguments->from_t = arg;
        break;
    case 2: // to_t
        arguments->to_t = arg;
        break;

    case 3:
        arguments->limits = 1;
        break;

    case 21:
        arguments->variable = arg;
        break;
    case 22:
        arguments->metric = arg;
        break;
    case 23:
        arguments->units = arg;
        break;

    case ARGP_KEY_ARG:
        if (state->arg_num >= MAX_ARGS) {
            /* Too many arguments. */
            argp_usage (state);
        }
        arguments->args[state->arg_num] = arg;
        break;

    case ARGP_KEY_END:
        if (state->arg_num < MIN_ARGS) {
            /* Not enough arguments. */
            argp_usage (state);
        }
        break;

    default:
        return ARGP_ERR_UNKNOWN;
    }
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE BOOL list_group_cb(
    void *user_data,
    wd_found_type type,     // type found
    char *fullpath,         // directory+filename found
    const char *directory,  // directory of found filename
    char *name,       // name of type found
    int level,              // level of tree where file found
    int index               // index of file inside of directory, relative to 0
)
{
    char *p = strrchr(directory, '/');
    if(p) {
        printf("  %s\n", p+1);
    } else {
        printf("  %s\n", directory);
    }
    return TRUE; // to continue
}

PRIVATE int list_groups(const char *path)
{
    printf("Groups found:\n");
    walk_dir_tree(
        path,
        "__simple_stats__.json",
        WD_RECURSIVE|WD_MATCH_REGULAR_FILE,
        list_group_cb,
        0
    );
    printf("\n");
    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
void print_summary(json_t *metrics)
{
    const char *var;
    json_t *jn_var;
    json_object_foreach(metrics, var, jn_var) {
        printf("Variable: %s\n", var);

        const char *metric;
        json_t *jn_metric;
        json_object_foreach(jn_var, metric, jn_metric) {
            printf("    Metric-Id: %s\n", metric);
            printf("    Period: %s\n", kw_get_str(jn_metric, "period", "", KW_REQUIRED));
            printf("    Units: %s\n", kw_get_str(jn_metric, "units", "", KW_REQUIRED));
            printf("    Compute: %s\n", kw_get_str(jn_metric, "compute", "", KW_REQUIRED));
            json_t *jn_data = kw_get_list(jn_metric, "data", 0, KW_REQUIRED);
            if(json_array_size(jn_data)) {
                int items = json_array_size(jn_data);
                json_t *jn_first = json_array_get(jn_data, 0);
                json_t *jn_last = json_array_get(jn_data, items-1);
                printf("        From %s\n",
                    kw_get_str(jn_first, "fr_d", "", KW_REQUIRED)
                );
                printf("        To   %s\n",
                    kw_get_str(jn_last, "to_d", "", KW_REQUIRED)
                );
            }
        }
    }
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE void print_keys(json_t *metrics)
{
    const char *key;
    json_t *jn_value;
    json_object_foreach(metrics, key, jn_value) {
        printf("        %s\n", key);
    }
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE void print_units(json_t *variable)
{
    const char *key;
    json_t *jn_value;
    json_object_foreach(variable, key, jn_value) {
        printf("        %s\n", json_string_value(json_object_get(jn_value, "units")));
    }

}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE void print_limits(json_t *jn_metric)
{
    json_t *jn_data = kw_get_list(jn_metric, "data", 0, KW_REQUIRED);
    if(json_array_size(jn_data)) {
        int items = json_array_size(jn_data);
        json_t *jn_first = json_array_get(jn_data, 0);
        json_t *jn_last = json_array_get(jn_data, items-1);
        printf("        From %s\n",
            kw_get_str(jn_first, "fr_d", "", KW_REQUIRED)
        );
        printf("        To   %s\n",
            kw_get_str(jn_last, "to_d", "", KW_REQUIRED)
        );
    }
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int _list_stats(
    char *path,
    char *group_name,
    json_t *match_cond,
    int verbose)
{
    /*-------------------------------*
     *  Open group
     *-------------------------------*/
    json_t *jn_stats = json_pack("{s:s, s:s}",
        "path", path,
        "group", group_name
    );
    json_t * stats = rstats_open(jn_stats);
    if(!stats) {
        fprintf(stderr, "Can't open stats %s/%s\n\n", path, group_name);
        exit(-1);
    }

    json_t *metrics = rstats_metrics(stats);
    if(verbose) {
        print_json(metrics);
    }

    if(kw_get_bool(match_cond, "show_limits", 0, 0)) {
        const char *var;
        json_t *jn_v;
        json_object_foreach(metrics, var, jn_v) {
            printf("   Variable: %s\n", var);
            const char *metr;
            json_t *jn_metr;
            json_object_foreach(jn_v, metr, jn_metr) {
                printf("   Metrica: %s, Units: %s\n", metr, kw_get_str(jn_metr, "units", "", 0));
                print_limits(jn_metr);
            }
        }
        /*
         *  Free resources
         */
        json_decref(metrics);
        JSON_DECREF(match_cond);
        rstats_close(stats);
        return -1;
    }

    uint64_t from_t = kw_get_int(match_cond, "from_t", 0, KW_WILD_NUMBER);
    uint64_t to_t = kw_get_int(match_cond, "to_t", 0, KW_WILD_NUMBER);
    const char *variable = kw_get_str(match_cond, "variable", "", 0);
    const char *metric_name = kw_get_str(match_cond, "metric", "", 0);
    const char *units = kw_get_str(match_cond, "units", "", 0);

    if(empty_string(variable)) {
        printf("What Variable?\n");
        printf("    Available Variables:\n");
        print_keys(metrics);

        /*
         *  Free resources
         */
        json_decref(metrics);
        JSON_DECREF(match_cond);
        rstats_close(stats);
        return -1;
    }

    json_t *jn_variable = kw_get_dict(metrics, variable, 0, 0);
    if(!jn_variable) {
        printf("Variable \"%s\" not found\n", variable);
        printf("    Available Variables:\n");
        print_keys(metrics);

        /*
         *  Free resources
         */
        json_decref(metrics);
        JSON_DECREF(match_cond);
        rstats_close(stats);
        return -1;
    }

    json_t *metric = 0;
    if(!empty_string(units)) {
        metric = find_metric_by_units(metrics, variable, units, FALSE);
        if(!metric) {
            printf("What Units?\n");
            printf("    Available Units:\n");
            print_units(jn_variable);

            /*
             *  Free resources
             */
            json_decref(metrics);
            JSON_DECREF(match_cond);
            rstats_close(stats);
            return -1;
        }
    }
    if(!metric) {
        if(empty_string(metric_name)) {
            printf("What Metric or Units?\n");
            printf("    Available Metrics:\n");
            print_keys(jn_variable);
            printf("    Available Units:\n");
            print_units(jn_variable);

            /*
            *  Free resources
            */
            json_decref(metrics);
            JSON_DECREF(match_cond);
            rstats_close(stats);
            return -1;
        }

        metric = rstats_metric(metrics, variable, metric_name, "", FALSE);
        if(!metric) {
            printf("What Metric?\n");
            printf("    Available Metrics:\n");
            print_keys(jn_variable);

            /*
             *  Free resources
             */
            json_decref(metrics);
            JSON_DECREF(match_cond);
            rstats_close(stats);
            return -1;
        }
    }

    if(!from_t && !to_t) {
        printf("What Range?\n");
        printf("    Available Limits:\n");
        print_limits(metric);

        /*
         *  Free resources
         */
        json_decref(metrics);
        JSON_DECREF(match_cond);
        rstats_close(stats);
        return -1;
    }

    if(!to_t) {
        //to_t = time((time_t *)&to_t);
        to_t = (uint64_t)-1;
    }
    if(!from_t) {
        from_t = 1;
    }

    json_t *jn_data = rstats_get_data(metric, from_t, to_t);
    if(jn_data) {
        print_json(jn_data);
        json_decref(jn_data);
    }

    /*
     *  Free resources
     */
    json_decref(metrics);
    JSON_DECREF(match_cond);
    rstats_close(stats);

    return 0;
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int list_stats(
    char *path,
    char *group,
    json_t *match_cond,
    int verbose)
{
    /*
     *  Check if path contains all
     */
    char bftemp[PATH_MAX];
    snprintf(bftemp, sizeof(bftemp), "%s%s%s",
        path,
        (path[strlen(path)-1]=='/')?"":"/",
        "__simple_stats__.json"
    );
    if(is_regular_file(bftemp)) {
        pop_last_segment(bftemp); // pop __simple_stats__.json
        group = pop_last_segment(bftemp);
        if(!empty_string(group)) {
            return _list_stats(
                bftemp,
                group,
                match_cond,
                verbose
            );
        }
    }

    if(empty_string(group)) {
        fprintf(stderr, "What Stats Group?\n\n");
        list_groups(path);
        exit(-1);
    }

    return _list_stats(
        path,
        group,
        match_cond,
        verbose
    );
}

/***************************************************************************
 *
 ***************************************************************************/
PRIVATE BOOL list_recursive_group_cb(
    void *user_data,
    wd_found_type type,     // type found
    char *fullpath,         // directory+filename found
    const char *directory,  // directory of found filename
    char *name,             // name of type found
    int level,              // level of tree where file found
    int index               // index of file inside of directory, relative to 0
)
{
    list_params_t *list_params = user_data;
    list_params_t list_params2 = *list_params;

    char *p = strrchr(directory, '/');
    if(p) {
        snprintf(list_params2.group, sizeof(list_params2.group), "%s", p+1);
    } else {
        snprintf(list_params2.group, sizeof(list_params2.group), "%s", directory);
    }

    printf("\n====> Stats Group: %s\n", list_params2.group);

    partial_counter = 0;
    JSON_INCREF(list_params2.match_cond);
    _list_stats(
        list_params2.path,
        list_params2.group,
        list_params2.match_cond,
        list_params2.verbose
    );
    //printf("\n====> %s: %d records\n", list_params2.group, partial_counter);

    return TRUE; // to continue
}

PRIVATE int list_recursive_groups(list_params_t *list_params)
{
    walk_dir_tree(
        list_params->path,
        ".*\\__simple_stats__\\.json",
        WD_RECURSIVE|WD_MATCH_REGULAR_FILE,
        list_recursive_group_cb,
        list_params
    );
    JSON_DECREF(list_params->match_cond);

    return 0;
}


/***************************************************************************
 *
 ***************************************************************************/
PRIVATE int list_recursive(
    char *path,
    char *group,
    json_t *match_cond,
    int verbose)
{
    list_params_t list_params;
    memset(&list_params, 0, sizeof(list_params));

    snprintf(list_params.path, sizeof(list_params.path), "%s", path);
    if(group) {
        snprintf(list_params.group, sizeof(list_params.group), "%s", group);
    }
    list_params.match_cond = match_cond;
    list_params.verbose = verbose;

    if(empty_string(group)) {
        return list_recursive_groups(&list_params);
    }

    return list_stats(
        path,
        group,
        match_cond,
        verbose
    );
}

/***************************************************************************
 *                      Main
 ***************************************************************************/
int main(int argc, char *argv[])
{
    /*
     *  Default values
     */
    memset(&arguments, 0, sizeof(arguments));

    /*
     *  Parse arguments
     */
    argp_parse(&argp, argc, argv, 0, 0, &arguments);

    uint64_t MEM_MAX_SYSTEM_MEMORY = free_ram_in_kb() * 1024LL;
    MEM_MAX_SYSTEM_MEMORY /= 100LL;
    MEM_MAX_SYSTEM_MEMORY *= 90LL;  // Coge el 90% de la memoria

    uint64_t MEM_MAX_BLOCK = (MEM_MAX_SYSTEM_MEMORY / sizeof(md_record_t)) * sizeof(md_record_t);

    MEM_MAX_BLOCK = MIN(1*1024*1024*1024LL, MEM_MAX_BLOCK);  // 1*G max

    gbmem_startup_system(
        MEM_MAX_BLOCK,
        MEM_MAX_SYSTEM_MEMORY
    );
    json_set_alloc_funcs(
        gbmem_malloc,
        gbmem_free
    );

    log_startup(
        NAME,       // application name
        VERSION,    // applicacion version
        NAME        // executable program, to can trace stack
    );
    log_add_handler(NAME, "stdout", LOG_OPT_UP_WARNING, 0);

    /*----------------------------------*
     *  Match conditions
     *----------------------------------*/
    json_t *match_cond = json_object();

    if(arguments.from_t) {
        timestamp_t timestamp;
        int offset;
        if(all_numbers(arguments.from_t)) {
            timestamp = atoll(arguments.from_t);
        } else {
            parse_date_basic(arguments.from_t, &timestamp, &offset);
        }
        json_object_set_new(match_cond, "from_t", json_integer(timestamp));
    }
    if(arguments.to_t) {
        timestamp_t timestamp;
        int offset;
        if(all_numbers(arguments.to_t)) {
            timestamp = atoll(arguments.to_t);
        } else {
            parse_date_basic(arguments.to_t, &timestamp, &offset);
        }
        json_object_set_new(match_cond, "to_t", json_integer(timestamp));
    }

    if(arguments.variable) {
        json_object_set_new(
            match_cond,
            "variable",
            json_string(arguments.variable)
        );
    }
    if(arguments.metric) {
        json_object_set_new(
            match_cond,
            "metric",
            json_string(arguments.metric)
        );
    }
    if(arguments.units) {
        json_object_set_new(
            match_cond,
            "units",
            json_string(arguments.units)
        );
    }
    if(arguments.limits) {
        json_object_set_new(
            match_cond,
            "show_limits",
            json_true()
        );
    }

    /*
     *  Do your work
     */
    if(empty_string(arguments.path)) {
        fprintf(stderr, "What Statistics path?\n");
        exit(-1);
    }
    if(arguments.recursive) {
        list_recursive(
            arguments.path,
            arguments.group,
            match_cond,
            arguments.verbose
        );
    } else {
        list_stats(
            arguments.path,
            arguments.group,
            match_cond,
            arguments.verbose
        );
    }

    gbmem_shutdown();
    log_end();

    return 0;
}
