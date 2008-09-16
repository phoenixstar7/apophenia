/** \file apop_settings.c Specifying model characteristics and details of estimation methods.
 
Copyright (c) 2008 by Ben Klemens.  Licensed under the modified GNU GPL v2; see COPYING and COPYING2.  */
#include "types.h"
#include "settings.h"


static size_t get_settings_ct(apop_model *model){
  int ct =0;
    if (!model->settings) return 0;
    while (strlen(model->settings[ct].name)) ct++;
    return ct;
}

/** Remove a settings group from a model.

  There are two ways to use this, the function or the macro:
  \code
  apop_settings_rm_group(your_model, "apop_mle");
  //or
  Apop_settings_rm_group(your_model, apop_mle);
  \endcode

  The macro just calls the function, but is in line with some of the other macros that are preferred over the function.

  If the model has no settings or your preferred settings group is not found, this function does nothing.

  \ingroup settings
 */
void apop_settings_rm_group(apop_model *m, char *delme){
  //apop_assert_void(m->settings, 2, 'c', "The model had no settings, so nothing was removed.");
  if (!m->settings)  
      return;
  int i = 0, j;
  int ct = get_settings_ct(m);
 
    while (strlen(m->settings[i].name)){
        if (!strcmp(m->settings[i].name, delme)){
            ((void (*)(void*))m->settings[i].free)(m->settings[i].setting_group);
            for (j=i+1; j< ct+1; j++){//don't forget the null sentinel.
                strcpy(m->settings[j-1].name, m->settings[j].name);
                m->settings[j-1].free = m->settings[j].free;
                m->settings[j-1].copy = m->settings[j].copy;
                m->settings[j-1].setting_group = m->settings[j].setting_group;
            }
            i--;
        }
        i++;
    }
   // apop_assert_void(0, 1, 'c', "I couldn't find %s in the input model, so nothing was removed.", delme);
}

/** Don't use this function. It's what the \c Apop_settings_add_group macro uses internally. Use that.  */
void apop_settings_group_alloc(apop_model *model, char *type, void *free_fn, void *copy_fn, void *the_group){
    if(apop_settings_get_group(model, type))  
        apop_settings_rm_group(model, type); 
    int ct = get_settings_ct(model);
    model->settings = realloc(model->settings, sizeof(apop_settings_type)*(ct+2));   
    strncpy(model->settings[ct].name, type, 100);
    model->settings[ct].setting_group = the_group;
    model->settings[ct].free = free_fn; 
    model->settings[ct].copy = copy_fn;
    model->settings[ct+1].name[0] = '\0';
    model->settings[ct+1].free = NULL;
    model->settings[ct+1].copy = NULL;
    model->settings[ct+1].setting_group = NULL;
}

/** This function gets the settings group with the given name. If it
  isn't found, then it returns NULL, so you can easily put it in a conditional like 
  \code 
  if (!apop_settings_get_group(m, "apop_ols")) ...
  \endcode

  The settings macros don't need quotation marks, e.g. 
  \code 
  if (!Apop_settings_get_group(m, apop_ols)) ...
  \endcode
  It is recommended that you stick with this form, because other operations on settings require this form.
*/
void * apop_settings_get_group(apop_model *m, char *type){
  int   i = 0;
    if (!m->settings) return NULL;
    while (strlen(m->settings[i].name)){
       if (!strcmp(type, m->settings[i].name))
           return m->settings[i].setting_group;
       i++;
    }
    if (!strlen(type)) //requesting the blank sentinel.
       return m->settings[i].setting_group;
    return NULL;
}

/** Copy a settings group with the given name from the second model to
 the first.  (i.e., the arguments are in memcpy order). */
void apop_settings_copy_group(apop_model *outm, apop_model *inm, char *copyme){
  apop_assert_void(inm->settings, 0, 's', "The input model (i.e., the second argument to this function) has no settings.\n");
  void *g =  apop_settings_get_group(inm, copyme);
  if (strlen(copyme))
      apop_assert_void(g, 0, 's', "I couldn't find the group %s in "
                                    "the input model (i.e., the second argument to this function).\n", copyme);
  int i=0;
    while (strlen(inm->settings[i].name)){
       if (!strcmp(copyme, inm->settings[i].name))
           break;
       i++;
    }
    void *gnew;
    if (inm->settings[i].copy)
        gnew = ((void *(*)(void*))inm->settings[i].copy)(g);
    else
        gnew = g;
    apop_settings_group_alloc(outm, copyme, inm->settings[i].free, inm->settings[i].copy, gnew);
}
