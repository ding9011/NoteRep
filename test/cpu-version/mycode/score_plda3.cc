#include "mylib/sre.h"
#include "ivector/plda.h"

using namespace kaldi;
void trim(string &);

void trim(string &s){
	if (s.empty()) return;
	int index = 0;
	for(; index < s.size(); index++)
		if(char(s[index]) < 58 && char(s[index]) > 47) break;
	s.erase(0, index);
	for(index = s.size() - 1; index > -1; index--)
		if(char(s[index] < 58) && char(s[index] > 47)) break;
	s.erase(index + 1, s.size());
}

template<typename T>
void transform_ivector(Matrix<T> transform, Vector<T> &ivector){
//	KALDI_LOG << "transform rows:" << transform.NumRows() << "\ttransform cols:" << transform.NumCols();
	SubMatrix<T> linear_term(transform,
							0, transform.NumRows(),
							0, transform.NumCols() - 1);
	Vector<T> constant_term(transform.NumRows());
	constant_term.CopyColFromMat(transform, transform.NumCols() - 1);
	Vector<T> transformed_ivector(transform.NumRows());
	if (ivector.Dim() == transform.NumCols()) {
        transformed_ivector.AddMatVec(1.0, transform, kNoTrans, ivector, 0.0);
    } else {
        KALDI_ASSERT(ivector.Dim() == transform.NumCols() - 1);
        transformed_ivector.CopyFromVec(constant_term);
        transformed_ivector.AddMatVec(1.0, linear_term, kNoTrans, ivector, 1.0);
    }
	ivector = transformed_ivector;
}

int main(int argc, char *argv[]) {
	typedef kaldi::int32 int32;
	typedef kaldi::int64 int64;
	try {
		const char *usage =
			"Computes dot-products between two iVectors;\n"
			"Usage: wav_score <train_ivector> <test_ivector> <plda-file> <transform.mat>\n"
			"example: ./wav_score_plda3 output/6311071945369651908_3.txt output/6311179250832579703_7.txt plda200 LCC900cx2000ivec200/transform.mat\n";
		ParseOptions po(usage);
		PldaConfig plda_opts;
		PldaUnsupervisedAdaptorConfig plda_adptor_config;
	    plda_adptor_config.Register(&po);
		plda_opts.Register(&po);
		po.Read(argc, argv);
		if (po.NumArgs() != 4) {
			po.PrintUsage();
			return -1;
		}
		std::string file1 = po.GetArg(1), file2 = po.GetArg(2);
		std::string plda_file = po.GetArg(3);
		std::string tran_mat_file = po.GetArg(4);
		std::fstream _file;
		_file.open(file1.c_str(), std::ios::in);
		if(!_file) {
			KALDI_WARN << "\ncan't find train ivector file" << std::endl;
			throw false;
		}
		_file.close();
		_file.open(file2.c_str(), std::ios::in);
		if(!_file) {
			KALDI_WARN << "\ncan't find test ivector file" << std::endl;
			throw false;
		}
		_file.close();
		_file.open(plda_file.c_str(), std::ios::in);
		if(!_file) {
			KALDI_WARN << "\ncan't find plda file" << std::endl;
			throw false;
		}
		_file.close();
		_file.open(tran_mat_file.c_str(), std::ios::in);
		if(!_file) {
			KALDI_WARN << "\ncan't find transform mat file" << std::endl;
			throw false;
		}
		_file.close();
		long long start, end, timeuse;
		start = getSystemTime();
		Input k1(file1), k2(file2);
		std::string text1, text2;
		std::string line;
		while (std::getline(k1.Stream(), line)) {
			text1 += line;
		}
		while (std::getline(k2.Stream(), line)) {
			text2 += line;
		}
		if (text1.empty() || text2.empty()) {
			KALDI_WARN << "empty ivector file" << std::endl;
			return -1;
		}
		trim(text1);
		trim(text2);
		std::vector<BaseFloat> fields;
		SplitStringToFloats(text1, " \t\n\r", true, &fields);
		Vector<BaseFloat> train_ivector;
		train_ivector.Resize(fields.size());
		train_ivector.CopyFromPtr(&fields[0], fields.size());
		SplitStringToFloats(text2, " \t\n\r", true, &fields);
		Vector<BaseFloat> test_ivector;
		test_ivector.Resize(fields.size());
		test_ivector.CopyFromPtr(&fields[0], fields.size());
		//Read plda
		start = getSystemTime();
		Plda plda;
		ReadKaldiObject(plda_file, &plda);
		end = getSystemTime();
		timeuse = end - start;
		KALDI_LOG << "read plda file time: " << timeuse << "ms";
		start = getSystemTime();
		//plda-adapt train_ivector
		PldaUnsupervisedAdaptor adaptor;
		adaptor.AddStats(1.0, train_ivector);
		adaptor.UpdatePlda(plda_adptor_config, &plda);
		end = getSystemTime();
		timeuse = end - start;
		KALDI_LOG << "plda adapt time: " << timeuse << "ms";
		//normalize
		KALDI_LOG << "train ivector dim:" << train_ivector.Dim() << "\t test ivector dim:" << test_ivector.Dim();
		ivector_normalize_length(train_ivector);
		ivector_normalize_length(test_ivector);
		//transform
		Matrix<BaseFloat> transform;
		ReadKaldiObject(tran_mat_file, &transform);
		ivector_normalize_length(train_ivector);
		transform_ivector(transform, train_ivector);
		transform_ivector(transform, test_ivector);
		ivector_normalize_length(train_ivector);
		ivector_normalize_length(test_ivector);
		KALDI_LOG << "train ivector dim:" << train_ivector.Dim() << "\t test ivector dim:" << test_ivector.Dim();
		start = getSystemTime();
		int32 dim = plda.Dim();
		//transform ivector
		Vector<BaseFloat> *transformed_train_ivector = new Vector<BaseFloat>(dim);
		Vector<BaseFloat> *transformed_test_ivector = new Vector<BaseFloat>(dim);
		plda.TransformIvector(plda_opts, train_ivector, transformed_train_ivector);
		plda.TransformIvector(plda_opts, test_ivector, transformed_test_ivector);
		//compute score
		Vector<double> train_ivector_d(*transformed_train_ivector), test_ivector_d(*transformed_test_ivector);
		BaseFloat score = plda.LogLikelihoodRatio(train_ivector_d,
												  1,
												  test_ivector_d);
		end = getSystemTime();
		timeuse = end - start;
		KALDI_LOG << "plda scoring time: " << timeuse << "ms";
		std::cout << score << std::endl;
//		BaseFloat dot_prod = VecVec(train_ivector, test_ivector);
//		std::cout << "dot prod:" << dot_prod << std::endl;
	} catch(const std::exception &e) {
		std::cerr << e.what();
		std::cout << "error" << std::endl;
		return -1;
	}
}
