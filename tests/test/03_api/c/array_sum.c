/*************************************************************************************/
/*      Copyright 2015 Barcelona Supercomputing Center                               */
/*                                                                                   */
/*      This file is part of the NANOS++ library.                                    */
/*                                                                                   */
/*      NANOS++ is free software: you can redistribute it and/or modify              */
/*      it under the terms of the GNU Lesser General Public License as published by  */
/*      the Free Software Foundation, either version 3 of the License, or            */
/*      (at your option) any later version.                                          */
/*                                                                                   */
/*      NANOS++ is distributed in the hope that it will be useful,                   */
/*      but WITHOUT ANY WARRANTY; without even the implied warranty of               */
/*      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the                */
/*      GNU Lesser General Public License for more details.                          */
/*                                                                                   */
/*      You should have received a copy of the GNU Lesser General Public License     */
/*      along with NANOS++.  If not, see <http://www.gnu.org/licenses/>.             */
/*************************************************************************************/

/*************************************************************************************/
/*                                                                                   */
/*      Formerly this test was OpenMP aplication which computes the sum of an        */
/*      array. The code is generated by Mercurium C Compiler (MCC) to use            */
/*      Nanos++ Runtime Library                                                      */
/*                                                                                   */
/*************************************************************************************/

/*
<testinfo>
test_generator=gens/api-generator
</testinfo>
*/

#include <nanos.h>
#include <stdlib.h>

void array_sum(int array_data[16834], int result[1], int tid)
{
    int l_i, l_j;
    printf("Execute a task %d\n", tid);
    result[0] = 0;
    for (l_i = 0;
        l_i < 16834;
        l_i++)
    {
        result[0] += array_data[l_i];
    }
}

typedef struct _nx_data_env_0_t_tag
{
        int * * l_array_of_arrays_0;
        int * l_partial_sums_0;
        int l_i_0;
} _nx_data_env_0_t;

int main(int argc, char * argv[]);
void _smp__ol_main_0(_nx_data_env_0_t * _args)
{
    array_sum((_args->l_array_of_arrays_0)[(_args->l_i_0)], &((_args->l_partial_sums_0)[(_args->l_i_0)]), (_args->l_i_0));
}

typedef struct {
   nanos_wd_props_t props;
   size_t data_alignment;
   size_t num_copies;
   size_t num_devices;
   size_t num_dimensions;
   char * description;
   nanos_device_t devices[];
} nanos_const_wd_definition_local_t;

nanos_const_wd_definition_local_t const_data1 = 
{
   { .tied = 1},
   0, 0, 1, 0, NULL,
   { { nanos_smp_factory, 0 } }
};


int main(int argc, char * argv[])
{
    int * * l_array_of_arrays;
    int * l_partial_sums;
    int l_num_procs;
    int l_total;
    int l_i, l_j;
    if (argc != 2)
    {
        printf("Usage: %s number_of_processors\n", argv[0]);
        return 0;
    }
    l_num_procs = atoi(argv[1]);
    if (l_num_procs < 1 && l_num_procs > 16)
    {
        printf("The number of processors must be between 1 and 16\n");
        return 0;
    }
    l_partial_sums = (int *) malloc(l_num_procs * sizeof(int));
    l_array_of_arrays = (int **) malloc(l_num_procs * sizeof(int *));
    for (l_i = 0;
        l_i < l_num_procs;
        l_i++)
    {
        l_array_of_arrays[l_i] = (int *) malloc(16834 * sizeof(int));
        for (l_j = 0;
            l_j < 16834;
            l_j++)
        {
            if ((l_j % 2) == 0)
                l_array_of_arrays[l_i][l_j] = 1;
            else
                l_array_of_arrays[l_i][l_j] = 0;
        }
    }
    for (l_i = 0;
        l_i < l_num_procs;
        l_i++)
    {
        {
            nanos_smp_args_t _ol_main_0_smp_args = {
                (void (*)(void *)) _smp__ol_main_0
            };
            _nx_data_env_0_t * ol_args = (_nx_data_env_0_t *) 0;
            nanos_wd_t wd = (nanos_wd_t) 0;
            const_data1.data_alignment = __alignof__(_nx_data_env_0_t);
            const_data1.devices[0].arg = &_ol_main_0_smp_args;
            nanos_wd_dyn_props_t dyn_data1 = { 0 };
            nanos_err_t err;
            err = nanos_create_wd_compact(&wd, (nanos_const_wd_definition_t *) &const_data1, &dyn_data1, sizeof(_nx_data_env_0_t), (void **) &ol_args, nanos_current_wd(), (nanos_copy_data_t **) 0, NULL);
            if (err != NANOS_OK)
                nanos_handle_error(err);
            if (wd != (nanos_wd_t) 0)
            {
                ol_args->l_array_of_arrays_0 = l_array_of_arrays;
                ol_args->l_partial_sums_0 = l_partial_sums;
                ol_args->l_i_0 = l_i;
                err = nanos_submit(wd, 0, (nanos_data_access_t *) 0, (nanos_team_t) 0);
                if (err != NANOS_OK)
                    nanos_handle_error(err);
            }
            else
            {
                _nx_data_env_0_t imm_args;
                imm_args.l_array_of_arrays_0 = l_array_of_arrays;
                imm_args.l_partial_sums_0 = l_partial_sums;
                imm_args.l_i_0 = l_i;
                err = nanos_create_wd_and_run_compact((nanos_const_wd_definition_t *) &const_data1, &dyn_data1,  sizeof(_nx_data_env_0_t),
                       &imm_args, 0, (nanos_data_access_t *) 0, (nanos_copy_data_t *) 0, 0, NULL);
                if (err != NANOS_OK)
                    nanos_handle_error(err);
            }
        }
    }
    nanos_wg_wait_completion( nanos_current_wd(), 0 );
    l_total = 0;
    for (l_i = 0;
        l_i < l_num_procs;
        l_i++)
    {
        printf("%d -> %d\n", l_i, l_partial_sums[l_i]);
        l_total += l_partial_sums[l_i];
    }
    printf("Result = %d\n", l_total);
    return 0;
}
