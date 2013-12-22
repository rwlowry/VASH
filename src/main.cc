#include "vash.hh"

using namespace std;

void processSIFTPoints( vector<SIFTFeature> & storage, cBitmap & bitmap );
void saveCentroids( vector<SIFTFeature> & c, char * filename );
void saveDatabase( vector< pair<MovieFile, vector<VisualWord> > > & db, char * filename );

int main( int argc, char ** argv ){
	bool train = false;
	bool test = false;

	char fileset[128];
	char testfile[128];

	/* Parse parameters */
	if( argc != 3 ) return -1;	//Not enough parameters passed

	if( strcmp( argv[1], "train" ) == 0 ){
		train = true;
		strcpy( fileset, argv[2] );
	}

	if( strcmp( argv[1], "test" ) == 0 ){
		test = true;
		strcpy( testfile, argv[2] );
	}


	/* Training phase */
	if( train ){
		vector<pair<MovieFile,vector<SIFTFeature> > > dataset;	//The database
		vector<SIFTFeature> clustering;							//Copy of features for clustering alone

		//Decode all Videos, train SIFT Features
		ifstream queries;
		queries.open(fileset, ifstream::in|ifstream::binary );  //Contains location of all movies
		if( !queries.is_open() )	return -1;					//Could not open file

		queries.seekg( queries.beg );

		int i = 0;
		while( true ){
			char filename[128];
			queries.getline( filename, 128 );

			if( !queries ) break;

			cBitmap b;

			Movie m( filename );
			MovieFile f( filename, i++ );
			vector<SIFTFeature> sift;

			while( m.loadNextFrame( b ) ){

				processSIFTPoints( sift, b );
				clustering.insert( clustering.end(), sift.begin(), sift.end() );
			}

			dataset.push_back( make_pair( f, sift ) );
		}

		queries.close();

		//Run k-means on ALL SIFT Features to generate words
		vector<SIFTFeature> centroids;

		KMeansClustering c(CLUSTER_K);
		c.lloyds( clustering );

		c.getCentroids( centroids );
		
		//Save centroids (Visual Words) to File
		char centroidFile[128];
		strcpy( centroidFile, "centroids.db" );

		saveCentroids( centroids, centroidFile );

		//Save visual word representation to database
		char datasetFile[128];
		strcpy( datasetFile, "dataset.db" );

		vector<pair<MovieFile, vector<VisualWord> > > db;
		for( vector<pair<MovieFile,vector<SIFTFeature> > >::iterator it = dataset.begin(); it != dataset.end(); it++ ){
			vector<VisualWord> v;

			for( vector<SIFTFeature>::iterator fit = it->second.begin(); fit != it->second.end(); fit++ ){
				VisualWord w;
				c.convertToVisualWord( w, *fit );
				v.push_back( w );
			}
			db.push_back( make_pair( it->first, v ) );
		}

		saveDatabase( db, datasetFile );
	}

	/* Test phase */
	if( test ){
		//Decode test video, train SIFT Features

		//Load VW, translate features

		//Compare representation to others in database

		//Output result
	}

	return 0;
}

//TODO: This should also save the VisualWords (->Merge centroids into VisualWord struct)
void saveCentroids( vector<SIFTFeature> & c, char * filename ){
	std::ofstream data;
	data.open(filename, ifstream::out|ifstream::binary );

	for( vector<SIFTFeature>::iterator it = c.begin(); it != c.end(); it++ ){
		data.write( reinterpret_cast<char *>(it->orientations[0].histogram), sizeof(SIFTDescriptor) );
	}
	data.close();
}

void saveDatabase( vector< pair<MovieFile, vector<VisualWord> > > & db, char * filename ){
	std::ofstream data;
	data.open(filename, ifstream::out|ifstream::binary );

	for( vector< pair<MovieFile, vector<VisualWord> > >::iterator it = db.begin(); it != db.end(); it++ ){
		int num = it->second.size();
		data.write( reinterpret_cast<char *>(&(it->first)), sizeof(MovieFile) );
		data.write( reinterpret_cast<char *>(&num), sizeof(int) );
		for( vector<VisualWord>::iterator fit = it->second.begin(); fit != it->second.end(); fit++ ){
			data.write( reinterpret_cast<char *>(&(*fit)), sizeof(VisualWord) );
		}
	}

	data.close();
}


/* For testing purposes, let's generate a few SIFT keypoints here. */
void processSIFTPoints( vector<SIFTFeature> & storage, cBitmap & bitmap ){

	int number_pixels = bitmap.getWidth()*bitmap.getHeight();

	unsigned char * buffer = new unsigned char[number_pixels];
	bitmap.getGreyscaleBitmap( buffer, number_pixels );		//SIFT accepts greyscale only

	/* Convert to correct format (float/vl_sift_pix) */
	vl_sift_pix * im = new vl_sift_pix[number_pixels];

	for( int i = 0; i < bitmap.getWidth()*bitmap.getHeight(); i++ )
		im[i] = static_cast<vl_sift_pix>( buffer[i]/256.0f );		//From testing: Both scaling to [0,1] or leaving at [0,255] are acceptable. vl_sift_pix = float.
	
	delete[] buffer;

	#ifdef DEBUG
	unsigned char * debug_im = new unsigned char[number_pixels*sizeof(Pixel)];
	bitmap.getBitmap( debug_im, number_pixels*sizeof(Pixel) );
	int iteration = 0;
	#endif

	/* Create SIFT Filter object */
	VlSiftFilt * s = vl_sift_new( bitmap.getWidth(), bitmap.getHeight(), 3, 3, 0 );
	vl_sift_process_first_octave( s, im );

	do{	
		/* Detect and retrieve keypoints in this octave */
		vl_sift_detect( s );
		const VlSiftKeypoint * keypoint = vl_sift_get_keypoints( s );
		int num_keys = vl_sift_get_nkeypoints( s );

		#ifdef DEBUG
		cout << num_keys << " keypoints were detected!" << endl;
		#endif

		for( int i = 0; i < num_keys; i++ ){
			#ifdef DEBUG
			cout << "Keypoint at x=" << ceil(keypoint[i].x) << ", y=" << ceil(keypoint[i].y) << ":" << endl;
			int coord = ceil(keypoint[i].x)+(ceil(keypoint[i].y)*bitmap.getWidth());				//For visualization of keypoints, you can set an appropriate color here
			debug_im[coord*4] = 255; debug_im[coord*4+1] = 255;
			debug_im[coord*4+2] = 0; debug_im[coord*4+3] = 0;
			#endif

			double angles[4];
			int num_angles = vl_sift_calc_keypoint_orientations( s, angles, &(keypoint[i]) );

			#ifdef DEBUG
			cout << "\t" << num_angles << " orientations found!" << endl;
			#endif

			if( num_angles == 0 )
				 continue;

			SIFTFeature f;
			f.num_orientations = num_angles;

			for( int j = 0; j < num_angles; j++ ){
				/* Get 128 bin histogram */
				vl_sift_pix * output = new vl_sift_pix[128];
				memset( output, 0, 128*sizeof( vl_sift_pix ) );

				vl_sift_calc_keypoint_descriptor( s, output, &(keypoint[i]), angles[j] );

				//Save feature here!
				memcpy( f.orientations[j].histogram, output, 128*sizeof( vl_sift_pix ) );

				delete[] output;
			}

			storage.push_back( f );
		}
	
		#ifdef DEBUG
		cout << "Iteration " << ++iteration << endl;
		#endif
	}while( vl_sift_process_next_octave( s ) != VL_ERR_EOF );

	#ifdef DEBUG
	glutViewer( debug_im, bitmap.getWidth(), bitmap.getHeight(), 4, 0, NULL, 600, 600 );	//Displays a bitmap image. See viewer.cc/hh
	//glutViewer is a blocking call. Program will stop here until the window is closed
	delete[] debug_im;
	#endif

	vl_sift_delete( s );
}
