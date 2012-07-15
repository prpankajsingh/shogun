/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * Written (W) 2012 Chiyuan Zhang
 * Copyright (C) 2012 Chiyuan Zhang
 */

#include <limits>
#include <algorithm>

#include <shogun/labels/BinaryLabels.h>
#include <shogun/multiclass/tree/RelaxedTreeUtil.h>
#include <shogun/multiclass/tree/RelaxedTree.h>


using namespace shogun;

CRelaxedTree::CRelaxedTree()
	:m_max_num_iter(3), m_svm_C(1), m_svm_epsilon(0.001), 
	m_kernel(NULL), m_feats(NULL), m_machine_for_confusion_matrix(NULL), m_num_classes(0)
{
	SG_ADD(&m_max_num_iter, "m_max_num_iter", "max number of iterations in alternating optimization", MS_NOT_AVAILABLE);
	SG_ADD(&m_svm_C, "m_svm_C", "C for svm", MS_AVAILABLE);
	SG_ADD(&m_svm_epsilon, "m_svm_epsilon", "epsilon for svm", MS_AVAILABLE);
}

CRelaxedTree::~CRelaxedTree()
{
	SG_UNREF(m_kernel);
	SG_UNREF(m_feats);
	SG_UNREF(m_machine_for_confusion_matrix);
}

CMulticlassLabels* CRelaxedTree::apply_multiclass(CFeatures* data)
{
	return NULL;
}

bool CRelaxedTree::train_machine(CFeatures* data)
{
	if (m_machine_for_confusion_matrix == NULL)
		SG_ERROR("Call set_machine_for_confusion_matrix before training");

	if (m_kernel == NULL)
		SG_ERROR("Assign a valid kernel before training");

	if (data)
	{
		CDenseFeatures<float64_t> *feats = dynamic_cast<CDenseFeatures<float64_t>*>(data);
		if (feats == NULL)
			SG_ERROR("Require non-NULL dense features of float64_t\n");
		set_features(feats);
	}

	CMulticlassLabels *lab = dynamic_cast<CMulticlassLabels *>(m_labels);

	RelaxedTreeUtil util;
	SGMatrix<float64_t> conf_mat = util.estimate_confusion_matrix(m_machine_for_confusion_matrix,
			m_feats, lab, m_num_classes);

	return false;
}

void CRelaxedTree::train_node(const SGMatrix<float64_t> &conf_mat, SGVector<int32_t> classes)
{
	std::vector<CRelaxedTree::entry_t> mu_init = init_node(conf_mat, classes);
	for (std::vector<CRelaxedTree::entry_t>::const_iterator it = mu_init.begin(); it != mu_init.end(); ++it)
	{
		CLibSVM *svm = train_node_with_initialization(*it);
		SG_UNREF(svm);
	}
}

CLibSVM *CRelaxedTree::train_node_with_initialization(const CRelaxedTree::entry_t &mu_entry)
{
	SGVector<int32_t> mu(m_num_classes), prev_mu(m_num_classes);
	mu.zero();
	mu[mu_entry.first.first] = 1;
	mu[mu_entry.first.second] = -1;

	CLibSVM *svm = new CLibSVM();
	SG_REF(svm);
	svm->set_C(m_svm_C, m_svm_C);
	svm->set_epsilon(m_svm_epsilon);

	for (int32_t iiter=0; iiter < m_max_num_iter; ++iiter)
	{
		SGVector<int32_t> subset(m_feats->get_num_vectors());
		SGVector<float64_t> binlab(m_feats->get_num_vectors());
		int32_t k=0;

		CMulticlassLabels *labs = dynamic_cast<CMulticlassLabels *>(m_labels);
		for (int32_t i=0; i < binlab.vlen; ++i)
		{
			int32_t lab = labs->get_int_label(i);
			binlab[i] = mu[lab];
			if (mu[lab] != 0)
				subset[k++] = i;
		}

		subset.vlen = k;

		CBinaryLabels *binary_labels = new CBinaryLabels(binlab);
		SG_REF(binary_labels);
		binary_labels->add_subset(subset);
		m_feats->add_subset(subset);

		m_kernel->init(m_feats, m_feats);
		svm->set_kernel(m_kernel);
		svm->set_labels(binary_labels);
		svm->train();

		binary_labels->remove_subset();
		m_feats->remove_subset();
		SG_UNREF(binary_labels);

		std::copy(&mu[0], &mu[mu.vlen], &prev_mu[0]);

		// TODO: color label space
		bool bbreak = true;
		for (int32_t i=0; i < mu.vlen; ++i)
		{
			if (mu[i] != prev_mu[i])
			{
				bbreak = true;
				break;
			}
		}

		if (bbreak)
			break;
	}

	return svm;
}

struct EntryComparator
{
	bool operator() (const CRelaxedTree::entry_t& e1, const CRelaxedTree::entry_t& e2)
	{
		return e1.second < e2.second;
	}
};
std::vector<CRelaxedTree::entry_t> CRelaxedTree::init_node(const SGMatrix<float64_t> &global_conf_mat, SGVector<int32_t> classes)
{
	// local confusion matrix
	SGMatrix<float64_t> conf_mat(classes.vlen, classes.vlen);
	for (index_t i=0; i < conf_mat.num_rows; ++i)
		for (index_t j=0; j < conf_mat.num_cols; ++j)
			conf_mat(i, j) = global_conf_mat(classes[i], classes[j]);

	// make conf matrix symmetry
	for (index_t i=0; i < conf_mat.num_rows; ++i)
		for (index_t j=0; j < conf_mat.num_cols; ++j)
			conf_mat(i,j) += conf_mat(j,i);

	int32_t num_entries = classes.vlen*(classes.vlen-1)/2;
	std::vector<CRelaxedTree::entry_t> entries;
	for (index_t i=0; i < classes.vlen; ++i)
		for (index_t j=i+1; j < classes.vlen; ++j)
			entries.push_back(std::make_pair(std::make_pair(i, j), conf_mat(i,j)));

	std::sort(entries.begin(), entries.end(), EntryComparator());

	const size_t max_n_samples = 30;
	int32_t n_samples = std::min(max_n_samples, entries.size());

	return std::vector<CRelaxedTree::entry_t>(entries.begin(), entries.begin() + n_samples);
}
