#ifndef _CFEATURES__H__
#define _CFEATURES__H__

#include "lib/common.h"
#include "lib/lapack.h"
#include "lib/common.h"
#include "preproc/PreProc.h"
#include "stdio.h"

class CPreProc;

class CFeatures
{
public:
	/** Features can 
	 * just be REALs, SHORT
	 * or STRINGs, FILES, or...
	*/

	CFeatures(long size);
	// copy constructor
	CFeatures(const CFeatures& orig);

	virtual ~CFeatures();

	/** return feature type with which objects derived 
	    from CFeatures can deal
	*/
	virtual EType get_feature_type()=0;
		
	/// set preprocessor
	virtual void set_preproc(CPreProc* p);

	/// get current preprocessor
	CPreProc* get_preproc();

	/// preprocess the feature_matrix
	virtual bool preproc_feature_matrix(bool force_preprocessing=false)=0;

	/// set/get the labels
	virtual bool set_label(long idx, int label) { return false ; }
	virtual int  get_label(long idx)=0 ;
	
	/// get label vector
	/// caller has to clean up
	int* get_labels(long &len) ;

	/// return the number of samples
	virtual long get_num_vectors()=0 ;

	virtual CFeatures* duplicate() const=0 ;

	virtual bool save(FILE* dest)=0;
	
protected:
	/// size of cache in MB
	long cache_size;

	/// Preprocessor
	CPreProc* preproc;

	/// true if features were already preprocessed
	bool preprocessed;

};
#endif
