//
//  Video.h
//  FaceTrackerTest
//
//  Created by josephchow on 2/6/20.
//

#ifndef Video_h
#define Video_h

#include "cinder/qtime/QuickTimeGl.h"
#include "cinder/app/App.h"
#include "cinder/Log.h"
using namespace ci;
using namespace std;
using namespace ci::app;
class Video {
public:
    Video(int width=640,int height=480){
        mFbo = gl::Fbo::create(app::getWindowWidth(),app::getWindowHeight());
        videoLoaded = false;
    }
    
    
    void loadVideo(fs::path moviePath){
        try {
            // load up the movie, set it to loop, and begin playing
            mMovie = qtime::MovieGl::create( moviePath );
          
            mMovie->play();
     
            console() << "Playing: " << mMovie->isPlaying() << std::endl;
        }
        catch( ci::Exception &exc ) {
            console() << "Exception caught trying to load the movie from path: " << moviePath << ", what: " << exc.what() << std::endl;
            mMovie.reset();
        }

        mFrameTexture.reset();
    }

    bool isVideoLoaded() { return videoLoaded; }
    bool isPlaying(){ return mMovie->isPlaying();}
    
    void update(){
        if( mMovie ){
            mFrameTexture = mMovie->getTexture();
           
        }
        
        draw();
    }
    
    gl::TextureRef getTexture(){
        return mFbo->getColorTexture();
    }
    
    ci::Surface8u getSurface(){
        return mFbo->readPixels8u(centeredRect.getInteriorArea());
    }
    
    Rectf getCenteredRect(){ return centeredRect; }
    
    // renders video into FBO.
    void draw(){

        gl::ScopedFramebuffer fbo(mFbo);
        if(mMovie && mMovie->isPlaying()){
            if( mFrameTexture ) {
                centeredRect = Rectf( mFrameTexture->getBounds() ).getCenteredFit( getWindowBounds(), true );
                gl::draw( mFrameTexture, centeredRect );
                videoLoaded = true;
                
            }
        }
    }
protected:
    bool videoLoaded;
    Rectf centeredRect;
    gl::TextureRef  mFrameTexture;
    qtime::MovieGlRef mMovie;
    gl::FboRef mFbo;
};

#endif /* Video_h */
