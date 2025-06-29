/**
    @file
    midi filter - patch midi filter
    doug hirlinger - doughirlinger@gmail.com

    @ingroup    examples
*/

#include "ext.h"
#include "ext_obex.h"
#include "ext_strings.h"
#include "ext_common.h"
#include <vector>
using namespace std;


// a c++ class representing a number, and types for a vector of those numbers
class number {
private:
    long value;
public:
    number(long &newValue)
    {
        value = newValue;
    }

    void setValue(const long &newValue)
    {
        value = newValue;
    }

    void getValue(long &retrievedValue)
    {
        retrievedValue = value;
    }
};
typedef std::vector<number>        numberVector;
typedef numberVector::iterator    numberIterator;

// max object instance data
typedef struct _midiFilter {
    t_object            ob;
    numberVector        *m_mainNotes;    // note: you must store this as a pointer and not directly as a member of the
    void                *m_outlet;
    void                *m_outlet2;
} t_midiFilter;


// prototypes
void    *midiFilter_new(t_symbol *s, long argc, t_atom *argv);
void    midiFilter_free(t_midiFilter *x);
void    midiFilter_assist(t_midiFilter *x, void *b, long m, long a, char *s);
void    midiFilter_bang(t_midiFilter *x);
void    midiFilter_count(t_midiFilter *x);
void    midiFilter_list(t_midiFilter *x, t_symbol *msg, long argc, t_atom *argv);
void    midiFilter_externalMidi(t_midiFilter *x, t_symbol *msg, long argc, t_atom *argv);
void    midiFilter_clear(t_midiFilter *x);
bool    midiFilter_contains(t_midiFilter *x, numberVector &collection, long targetValue);
bool    midiFilter_localMath(t_midiFilter *x, long value);
void    midiFilter_removeValue(t_midiFilter *x, numberVector &collection, long valueToRemove);
long    midiFilter_mainMath(t_midiFilter *x, long value);
void    midiFilter_version();
void    midiFilter_printReassigned(t_midiFilter *x);
void    midiFilter_isEmpty(t_midiFilter *x, long deviceID);


// globals
static t_class    *s_midiFilter_class = NULL;

/************************************************************************************/

void ext_main(void *r)
{
    t_class    *c = class_new("midiFilterMain",
                           (method)midiFilter_new,
                           (method)midiFilter_free,
                           sizeof(t_midiFilter),
                           (method)NULL,
                           A_GIMME,
                           0);

    class_addmethod(c, (method)midiFilter_bang,    "bang",            0);
    class_addmethod(c, (method)midiFilter_list,    "list",            A_GIMME,0);
    class_addmethod(c, (method)midiFilter_externalMidi,    "externalMidi",            A_GIMME,0);
    class_addmethod(c, (method)midiFilter_clear,    "clear",        0);
    class_addmethod(c, (method)midiFilter_count,    "count",        0);
    class_addmethod(c, (method)midiFilter_assist,    "assist",        A_CANT, 0);
    class_addmethod(c, (method)stdinletinfo,    "inletinfo",    A_CANT, 0);
    class_addmethod(c, (method)midiFilter_contains, "int",      A_LONG, 0);
    class_addmethod(c, (method)midiFilter_removeValue, "int", A_LONG, 0);
    class_addmethod(c, (method)midiFilter_mainMath, "int", A_LONG, 0);
    class_addmethod(c, (method)midiFilter_version, "version", 0);
    

    class_register(CLASS_BOX, c);
    s_midiFilter_class = c;
    
    post("midiFilterMain object 1.0");
}


/************************************************************************************/
// Object Creation Method

void *midiFilter_new(t_symbol *s, long argc, t_atom *argv)
{
    t_midiFilter    *x;

    x = (t_midiFilter *)object_alloc(s_midiFilter_class);
    if (x) {
       //systhread_mutex_new(&x->m_mutex, 0);
        x->m_outlet2 = listout((t_object *)x);
        x->m_outlet = outlet_new(x, NULL);
        x->m_mainNotes = new numberVector;
        x->m_mainNotes->reserve(220);
        midiFilter_list(x, gensym("list"), argc, argv);
    }
    return(x);
}


void midiFilter_free(t_midiFilter *x)
{
    //systhread_mutex_free(x->m_mutex);
    delete x->m_mainNotes;
}


/************************************************************************************/
// Methods bound to input/inlets

void midiFilter_assist(t_midiFilter *x, void *b, long msg, long arg, char *dst)
{
    if (msg==1)
        strcpy(dst, "deviceID, pitch, velocity from Locals");
    
    else if (msg==2) {
        
        if (arg==0) {
            strcpy(dst, "output to locals");
        } else if (arg==1) {
            strcpy(dst, "print to screen");
        }
        
    }
        
}



void midiFilter_bang(t_midiFilter *x)
{
    numberIterator iter, begin, end;
    int i = 0;
    long ac = 0;
    t_atom *av = NULL;
    long value;

//    systhread_mutex_lock(x->m_mutex);
    ac = x->m_mainNotes->size();

    if (ac)
        av = new t_atom[ac];

    if (ac && av) {
        
        begin = x->m_mainNotes->begin();
        end = x->m_mainNotes->end();

        iter = begin;

        for (;;) {
            
            (*iter).getValue(value);
            atom_setlong(av+i, value);

            i++;
            iter++;

            if (iter == end)
                break;
        }
//        systhread_mutex_unlock(x->m_mutex);    // must unlock before calling _clear() or we will deadlock

        outlet_anything(x->m_outlet2, gensym("list"), ac, av); // don't want to call outlets in mutexes either

        delete[] av;
        
    }
    //else
//        systhread_mutex_unlock(x->m_mutex);
}




void midiFilter_count(t_midiFilter *x)
{
    outlet_int(x->m_outlet, x->m_mainNotes->size());
}


void midiFilter_list(t_midiFilter *x, t_symbol *msg, long argc, t_atom *argv)
{
    //if there's an incoming list
    
    //systhread_mutex_lock(x->m_mutex);
    
    if (argc > 0) {
        
        long deviceID = atom_getlong(argv);
        long pitch = atom_getlong(argv+1);
        long velocity = atom_getlong(argv+2);
        long beforeMain;
        long firstReturnedPitch;
        long reassignedPitch;
        
        //if incoming pitch is note-on and mainNotes is empty then play and add to lists
        
        if (x->m_mainNotes->size() < 1 && velocity > 0) {
            
            x->m_mainNotes->push_back(pitch);
            outlet_list(x->m_outlet, NULL, 2, argv);
            
            return;
            
        //otherwise if mainNotes does exists and is note-on...
        
        } else if (x->m_mainNotes->size() > 0 && velocity > 0){
            
                beforeMain = pitch;
                pitch = midiFilter_mainMath(x, pitch);
                post("before1 %d", beforeMain);
                post("after1 %d", pitch);
                
                //if mainMath returns same pitch play and add to lists
                if (beforeMain == pitch) {
                    
                    x->m_mainNotes->push_back(pitch);
                    outlet_list(x->m_outlet, NULL, 2, argv);
                    return;
                    
                } else {
                    
                    if (pitch < 128) {
                        x->m_mainNotes->push_back(pitch);
                        atom_setlong(argv+1, pitch);
                        outlet_list(x->m_outlet, NULL, 3, argv);
                        outlet_anything(x->m_outlet, gensym("reassigned"), 3, argv);
                        
                        return;
                    }
                    else {
                        return;
                    }
                }
                return;
        }// if mainNotes > 0 && vel > 0
        //if is note-off
        if (velocity == 0) {
            //remove from main
            midiFilter_removeValue(x, *x->m_mainNotes, reassignedPitch);
            return;
        }
    } // if argv > 0
}

//for midi coming from other channels via send and receive objects

void midiFilter_externalMidi(t_midiFilter *x, t_symbol *msg, long argc, t_atom *argv)
{
    if (argc > 0) {
            
            long pitch = atom_getlong(argv);
            long velocity = atom_getlong(argv+1);
            
            //if incoming pitch is note-on and mainNotes is empty then add to mainNotes
            
            if (velocity > 0){
                
                x->m_mainNotes->push_back(pitch);
                
            } else if (velocity == 0){
                
                midiFilter_removeValue(x, *x->m_mainNotes, pitch);
                
            }
                
        }
}



void midiFilter_clear(t_midiFilter *x)
{
    x->m_mainNotes->clear();
}

bool midiFilter_contains(t_midiFilter *x, numberVector &collection, long targetValue)
{
    numberIterator iter, begin, end;
    long value;
    bool found = false;

    // Lock the mutex to ensure thread safety
    //systhread_mutex_lock(x->m_mutex);

    // Check if the collection is not empty
    if (!collection.empty()) {
        begin = collection.begin();
        end = collection.end();
        iter = begin;

        // Iterate through the collection to find the target value
        for (;;)
        {
            (*iter).getValue(value);
            if (value == targetValue) {
                found = true;
                break;
            }

            iter++;
            if (iter == end)
                break;
        }
    }

    return found;
}

long midiFilter_mainMath(t_midiFilter *x, long value)
{
    bool too_close;
    long candidate = value;

    do {
        too_close = false;

        for (numberIterator iter = x->m_mainNotes->begin(); iter != x->m_mainNotes->end(); ++iter) {
            long listValue;
            (*iter).getValue(listValue);

            long diff = std::abs(candidate - listValue);
            if (diff > 0 && diff < 3) {
                too_close = true;
                candidate++;  // increment and retry
                break;
            }
        }

    } while (too_close);

    return candidate;
}




void midiFilter_removeValue(t_midiFilter *x, numberVector &collection, long valueToRemove)
{
    // Lock the mutex to ensure thread safety
    //systhread_mutex_lock(x->m_mutex);

    numberIterator iter = collection.begin();

    // Iterate through the collection to find the element
    while (iter != collection.end())
    {
        long currentValue;
        (*iter).getValue(currentValue);

        // If the value matches, erase the element
        if (currentValue == valueToRemove) {
            iter = collection.erase(iter); // Erase returns the next iterator
            break; // Stop after removing the first matching element
        } else {
            ++iter; // Move to the next element
        }
    }

    // Unlock the mutex
    //systhread_mutex_unlock(x->m_mutex);
}

    
void midiFilter_version()
{
    post("midiFilterMain object 1.0");
}
