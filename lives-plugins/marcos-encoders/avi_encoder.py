#!/usr/bin/env python


import locale
locale.setlocale(locale.LC_NUMERIC,"")

"""
avi_encoder.py

Front-end to various programs needed to create AVI
movies with some image-enhancing capabilities through the use of
ImageMagick. Meant as a companion to LiVES (to possibly call
from within a plugin, see http://www.xs4all.nl/~salsaman/lives/ )
but can also be used as a stand-alone program.

Requires MPlayer, ImageMagick (GraphicsMagick might also
work), sox, lame, and Python 2.3.0 or greater. Note that
this program can encode a SNOW video stream (if mencoder
supports it), but that it is still highly experimental.
This is also true for h.264. Use at your own risk.

Copyright (C) 2004-2005 Marco De la Cruz (marco@reimeika.ca)
It's a trivial program, but might as well GPL it, so see:
http://www.gnu.org/copyleft/gpl.html for license.

See my vids at http://amv.reimeika.ca !
"""

version = '0.0.10'
convert = 'convert'
mencoder = 'mencoder'
lameenc = 'lame'
sox = 'sox'

usage = \
      """
avi_encoder.py -h
avi_encoder.py -V
avi_encoder.py -C
avi_encoder.py [-o out] [-p pre] [-d dir] [-a aspect] [-D delay]
               [-q|-v] [-t type] [-k] [-e [[-w dir] [-c geom] [-r geom]]]
               [-s sndfile] [-b sndrate] [-f fpscode] [-L lv1file]
               [firstframe lastframe]
      """

help = \
     """
SUMMARY (ver. %s):

Encodes a series of PNG or JPG images into an XviD/Snow/h.264 + MP3
(AVI) stream and is also capable of performing some simple image
enhacements using ImageMagick. The images and audio are assumed to
be in an uncompressed LiVES format, hence making this encoder
more suitable to be used within a LiVES plugin (but can also
be used directly on a LiVES temporary directory).

OPTIONS:

-h          for help.

-V          shows the version number.

-C          check external program dependencies and exit.

-o out      will save the video in file "out".
            Default: "'type'_movie.avi". See below for 'type'.

-p pre      the encoder assumes that sound and images are named using
            LiVES' conventions i.e. "audio" and "00000001.ext",
            "00000002.ext"... where "ext" is either "jpg" or "png".
            However, avi_encoder.py will create temporary files
            which can be saved for later use (for example, after
            enhancing the images with "-e" , which may take a long
            time). These temporary files will be named
            "pre_00000001.ext", etc... where "pre" is either "eimg"
            or "rimg" and will be kept if the "-k" option is used.
            avi_encoder.py can then be run over the enhanced images
            only by selecting the appropriate prefix ("eimg" or
            "rimg") here. See "-e" to see what each prefix means.
            Default: "" (an empty string).

-d dir      "dir" is the directory containing the source image
            files. These must be of the form "00000001.ext" or
            "pre_00000001.ext" as explained above.
            Default: "."

-a aspect   sets the aspect ratio of the resulting MPEG-4. This can be:

               1    1:1 display
               2    4:3 display
               3    16:9 display
               4    2.21:1 display

            Default: "2"

-D delay    delay the audio stream by "delay" ms. Must be positive.
            Default: "0"

-q          quiet operation, only complains on error.

-v          be verbose.

-t type     type of video created. The options here are:

               "hi":   a very high quality XviD file, suitable
                       for archiving images of 720x480.

               "mh":   medium-high quality XviD, which still allows
                       to watch small videos (352x240 and below)
                       fullscreen.

               "ml":   medium-low quality XviD, suitable for
                       most videos but in particular small videos
                       (352x240 and below) meant for online
                       distribution.

               "lo":   a low quality XviD format. Enlarging
                       small videos will result in noticeable
                       artifacts. Good for distributing over slow
                       connections.

               "hi_x", "mh_x", "ml_x", "lo_x": same as above.

               "hi_h", "mh_h", "ml_h", "lo_h": same as above, but
                                               using the h.264 codec.

               "hi_s", "mh_s", "ml_s", "lo_s": same as above, but
                                               the experimental
                                               SNOW codec.

            Default: "ml"

-e          perform some simple filtering/enhancement to improve image
            quality. The images created by using this option will be
            stored in the directory specified by the "-w" option (or
            "-d" if not present, see above) and each filename will be
            prefixed with "eimg" (see "-p" above). Using this option will
            take enormous amounts of disk space, and in reality does
            not do anything that cannot be done within LiVES itself.
            Using this option enables the following four:

            -w dir      "dir" is the working directory where the enhanced
                        images  will be saved. Although these can all
                        co-exist with the original LiVES files it is
                        recommended you use a different directory. Note
                        that temporary files may take a huge amount of
                        space, perhaps twice as much as the LiVES files
                        themselves. If this directory does not exist is
                        will be created.
                        Default: same as "-d".

            -k         keep the temporary files, useful to experiment
                       with different encoding types without having
                       to repeatedly enhance the images (which can be
                       very time-consuming). See "-p" above.

            -c geom    in addition to enhancing the images, crop them
                       to the specified geometry e.g. "688x448+17+11".
                       Filename prefix remains "eimg". Note that the
                       dimensions of the images should both be multiples
                       of "16", otherwise some players may show artifacts
                       (such as garbage or green bars at the borders).

            -r geom    this will create a second set of images resulting
                       from resizing the images that have already been
                       enhanced and possibly cropped (if "-c" has been
                       specified). The geometry here is simple a new
                       image size e.g. "352x240!". This second set will have
                       filenames prefixed with the string "rimg" (see "-p"
                       above). Note that the dimensions of the images
                       should both be multiples of "16", otherwise some
                       players may show artifacts (such as garbage or
                       green bars at the borders).

-s sndfile  name of the audio file. Can be either raw "audio" or
            "[/path/to/]soundfile.wav".
            Default: "audio" inside "dir" as specified by "-d".

-b sndrate  sample rate of the sound file in Hertz.
            Default: "44100".

-f fpscode  frame-rate code. Acceptable values are:

              1 - 24000.0/1001.0 (NTSC 3:2 pulldown converted FILM)
              2 - 24.0 (NATIVE FILM)
              3 - 25.0 (PAL/SECAM VIDEO / converted FILM)
              4 - 30000.0/1001.0 (NTSC VIDEO)
              5 - 30.0
              6 - 50.0 (PAL FIELD RATE)
              7 - 60000.0/1001.0 (NTSC FIELD RATE)
              8 - 60.0

            If the input value is a float (e.g. 5.0, 14.555, etc.)
            the encoder will use that as frame rate.

            Default: "4".

-L lv1file  use the images stored in the LiVES file "lv1file" (e.g.
            "movie.lv1"). Files will be extracted inside "dir" as
            specified by "-d". Using this option allows to encode
            a movie without having to restore it first with LiVES.
            Note that the encoder will not delete the extracted
            files after it has finished, this must be done manually
            afterwards. If frame code/rate (-f) and/or sound rate (-b)
            are not specified the defaults will be used (30000/1001 fps
            and 44100 Hz). These, however, may not be correct, in which
            case they should be specified explicitly. Furthermore, the
            .lv1 audio (if present) is always assumed to have a sample
            data size in 16-bit words with two channels, and to be signed
            linear (this is usually the case, however). If any of these
            latter parameters are incorrect the resulting file may be
            corrupted and encoding will have to be done using LiVES.

firstframe  first frame number. If less than eight digits long it will
            be padded with zeroes e.g. 234 -> 00000234. A prefix can
            be added using the "-p" option.
            Default: "00000001"

lastframe   last frame number, padded with zeroes if necessary, prefix
            set by "-p".
            Default: the last frame of its type in the "-d" directory,
                     where "type" is set by the prefix.

Note that either no frames or both first and last frames must be specified.

EXAMPLES:

Suppose you have restored a LiVES' .lv1 file (in either JPG or PNG format),
and that the images are stored in the directory "/tmp/livestmp/991319584/".
In this example the movie is assumed to be 16:9. Then, in order to create
an AVI file you can simply do the following:

   avi_encoder.py -d /tmp/livestmp/991319584 -o /movies/default.avi -a 3

and the clip "default.avi" will be created in "/movies/" with the correct
aspect ratio.

Suppose we want to make a downloadable version of the clip, small in size
but of good quality. The following command activates the generic enhancement
filter and resizes the images to "352x240". Note that these operations
are performed after cropping the originals a bit (using "704x464+5+6").
This is done because we are assuming that there is a black border around
the original pictures which we want to get rid of (this might distort the
images somewhat, so be careful about cropping/resizing, and remember to
use dimensions which are multiples of 16):

   avi_encoder.py -v -d /tmp/livestmp/991319584 -w /tmp/tmpavi \\
   -o /movies/download.avi -a 3 -k -e -c "704x464+5+6" -r "352x240"

Since we use the "-k" flag the enhanced images are kept (both full-size
crops and the resized ones) in "/tmp/tmpavi". Beware that this may consume
a lot of disk space (about 10x as much as the originals). The reason we
keep them is because the above may take quite a long time and we may want
to re-use the enhanced images. So, for example, creating a high-quality
clip at full size can be accomplished now as follows:

   avi_encoder.py -d /tmp/tmpavi -t hi -o /movies/archive.avi \\
   -s /tmp/livestmp/991319584/audio -a 3 -k -p eimg

If, for example, we only want to encode frames 100 to 150 we can run
the following:

   avi_encoder.py -v -d /tmp/tmpavi -o /movies/selection.avi \\
   -a 3 -k -p eimg 100 150

Note that no audio has been selected ("-s"). This is because
avi_encoder.py cannot trim the audio to the appropriate length.
You would need to use LiVES to do this.

To delete all the enhanced images you can just remove "/tmp/tmpavi".

If you notice that the video and audio become out of sync so that
near the end of the movie the video trails the audio by, say, 0.2s,
you can use the "-D" option as follows:

   avi_encoder.py -d /tmp/livestmp/991319584 -o test.avi -D 200

Suppose that you have "movie1.lv1", "movie2.lv1" and "movie3.lv1".
Batch-encoding can be done as follows (zsh-syntax):

   for i in movie?.lv1
   do
     mkdir /tmp/$i:r
     avi_encoder.py -d /tmp/$i:r -o /tmp/$i:r.avi -L $i
     rm -rf /tmp/$i:r
   done

This will generate the files "movie1.avi", "movie2.avi" and
"movie3.avi" in "/tmp". Note that is is not necessary to
specify whether the files are stored in JPG or PNG format,
and that potentially time-consuming options (e.g. "-e") may
be enabled. It is not necessary to have a working LiVES
installation to encode .lv1 files.
     """ % version

def run(command):
    """
    Run a shell command
    """

    if verbose:
        print 'Running: \n' + command + '\n=== ... ==='
        std = ''
    else:
        std = ' > /dev/null 2>&1'

    os.system(command + std)


def do_enhance():
    """
    Image cleanup/crop/resize routine. Generic, but seems to work
    well with anime :)
    """

    if not quiet:
        print 'Enhancing images... please wait, this might take long...'

    enh_opts = "-enhance -sharpen '0.0x0.5' -gamma 1.2 -contrast -depth 8"

    if cgeom:
        docrop = ' '.join(['-crop', "'%s!'" % cgeom])
    else:
        docrop = ''

    iframe = first_num
    while True:
        # File names are padded with zeroes to 8 characters
        # (not counting the .ext).
        frame = str(iframe).zfill(8)
        fname = os.path.join(img_dir, frame + ext)
        if not os.path.isfile(fname) or (iframe == last_num + 1):
            break
        eimg_fname = os.path.join(work_dir, 'eimg' + frame + '.png')
        rimg_fname = os.path.join(work_dir, 'rimg' + frame + '.png')
        command = ' '.join([convert, docrop, enh_opts, fname, eimg_fname])
        run(command)
        if rgeom:
            if os.path.exists(rimg_fname): os.remove(rimg_fname)
            shrink = ' '.join([convert, '-resize', "'%s!'" % rgeom, '-depth 8'])
            command = ' '.join([shrink, eimg_fname, rimg_fname])
            run(command)
        else:
            if os.path.exists(rimg_fname): os.remove(rimg_fname)
	    try:
            	os.symlink(eimg_fname, rimg_fname)
	    except (IOError, OSError):
		shutil.copy(eimg_fname, rimg_fname)
        iframe+=1


def do_encode():
    """
    Encode a series of images into an XviD/Snow or h.264 stream, multiplexing
    audio if necessary
    """

    if not quiet:
        print 'Creating "%s"-quality video' % vtype

    if verbose:
        std = ''
        lameinfo = '--verbose'
        meinfo = '-v'
        hinfo = ':log=-1'
    else:
        std = ' > /dev/null 2>&1'
        lameinfo = '--quiet'
        meinfo = '-quiet'
        hinfo = ''

    common_vopts = meinfo + ' -mf type=%s:fps=%s -mc 0 -noskip ' % (ext[1:], fps) + \
                  '-vf hqdn3d=2:1:3,pp=va:128:8/ha:128:8/dr,dsize=%s' % aspect

    if vtype[-1] == 's':
        codec_vopts = '-ovc lavc -lavcopts ' + \
                      'vcodec=snow:vstrict=-2:mbcmp=1:subcmp=1:cmp=1:mbd=2:v4mv' + \
                      ':qpel:pred=1:autoaspect:vqscale='
        cpass = ''
    elif vtype[-1] == 'h':
        codec_vopts = '-ovc x264 -x264encopts ' + \
                      'frameref=15:subq=5' + hinfo + \
                      ':bitrate='
        cpass = 'pass='
    else:
        codec_vopts = '-ovc xvid -xvidencopts qpel:chroma_me:chroma_opt' + \
                      ':max_bframes=1:autoaspect:hq_ac:vhq=4:bitrate='
        cpass = 'pass='

    vqual = vtype[0:-2]
    if vqual == 'hi':
        if vtype[-1] == 'x': rate = '2000:'
        if vtype[-1] == 's': rate = '2'
        if vtype[-1] == 'h': rate = '2000:'
        mencoder_opts = ' '.join([common_vopts, codec_vopts + rate + cpass])
        lameenc_opts = '--vbr-new -h -V 0'
    elif vqual == 'mh':
        if vtype[-1] == 'x': rate = '1000:'
        if vtype[-1] == 's': rate = '3'
        if vtype[-1] == 'h': rate = '1000:'
        mencoder_opts = ' '.join([common_vopts, codec_vopts + rate + cpass])
        lameenc_opts = '--vbr-new -h -V 2'
    elif vqual == 'ml':
        if vtype[-1] == 'x': rate = '500:'
        if vtype[-1] == 's': rate = '4'
        if vtype[-1] == 'h': rate = '500:'
        mencoder_opts = ' '.join([common_vopts, codec_vopts + rate + cpass])
        lameenc_opts = '--vbr-new -h -V 4'
    elif vqual == 'lo':
        if vtype[-1] == 'x': rate = '200:'
        if vtype[-1] == 's': rate = '6'
        if vtype[-1] == 'h': rate = '200:'
        mencoder_opts = ' '.join([common_vopts, codec_vopts + rate + cpass])
        lameenc_opts = '--vbr-new -h -V 6'


    if img_dir != work_dir and not enhance:
        source_dir = img_dir
    else:
        source_dir = work_dir

    if frame_range:
        frame_list = [img_pre + str(f).zfill(8) + ext \
                      for f in range(first_num, last_num + 1)]
        syml = 'temporary_symlink_'
        for iframe in frame_list:
            frfile = os.path.join(source_dir, iframe)
            frlink = os.path.join(source_dir, syml + iframe)
            if os.path.islink(frlink): os.remove(frlink)
            os.symlink(frfile, frlink)
    else:
        syml = ''

    aviopts = '-of avi -force-avi-aspect %s -ofps %s' % (aspect, fps)

    all_vars = {}
    all_vars.update(globals())
    all_vars.update(locals())
    
    if audio:
        if not quiet:
            print 'Creating "%s"-quality mp3 file' % vtype

        mp3 = tempfile.mkstemp('.mp3', '', work_dir)[1]
        if rawsndf:
            wav = tempfile.mkstemp('.wav', '', work_dir)[1]
            sox_opts = '-t raw -r %s -w -c 2 -s' % sndr
            command = ' '.join([sox, sox_opts, sndf, wav])
            run(command)
        else:
            wav = sndf
        command = ' '.join([lameenc, lameinfo, lameenc_opts, wav, mp3])
        run(command)

        avimp3 = '-audiofile %s -oac copy -audio-delay %s' % (mp3, delay)
    else:
        wav = 'dummy'
        mp3 = 'dummy'
        avimp3 = '-nosound'

    all_vars.update(locals())

    if not quiet: print 'Encoding and multiplexing...'
    if vtype[-1] == 's':
        command = """cd %(source_dir)s ; \\
%(mencoder)s %(aviopts)s %(avimp3)s "mf://%(syml)s%(img_pre)s*%(ext)s" %(mencoder_opts)s -o "%(vidname)s" %(std)s""" % all_vars
        run(command)
    else:
        command = """cd %(source_dir)s ; \\
%(mencoder)s %(aviopts)s -nosound "mf://%(syml)s%(img_pre)s*%(ext)s" %(mencoder_opts)s1 -o /dev/null %(std)s ; \\
%(mencoder)s %(aviopts)s %(avimp3)s "mf://%(syml)s%(img_pre)s*%(ext)s" %(mencoder_opts)s2 -o "%(vidname)s" %(std)s
""" % all_vars
        run(command)

    if os.path.exists(os.path.join(source_dir, 'divx2pass.log')):
        os.remove(os.path.join(source_dir, 'divx2pass.log'))
    if os.path.exists(os.path.join(source_dir, 'xvid-twopass.stats')):
        os.remove(os.path.join(source_dir, 'xvid-twopass.stats'))
    if rawsndf and os.path.exists(wav): os.remove(wav)
    if os.path.exists(mp3): os.remove(mp3)

    if frame_range:
        lframes = os.path.join(source_dir, syml)
        for frame in glob.glob(lframes + '*'):
            os.remove(frame)


def do_clean():
    """
    Delete enhanced files
    """

    if not quiet:
        print 'Deleting all enhanced images (if any)'

    eframes = os.path.join(work_dir, 'eimg')
    rframes = os.path.join(work_dir, 'rimg')
    for frame in glob.glob(eframes + '*'):
         os.remove(frame)
    for frame in glob.glob(rframes + '*'):
         os.remove(frame)


def which(command):
    """
    Finds (or not) a command a la "which"
    """

    command_found = False

    if command[0] == '/':
        if os.path.isfile(command) and \
               os.access(command, os.X_OK):
            command_found = True
            abs_command = command
    else:
        path = os.environ.get('PATH', '').split(os.pathsep)
        for dir in path:
            abs_command = os.path.join(dir, command)
            if os.path.isfile(abs_command) and \
               os.access(abs_command, os.X_OK):
                command_found = True
                break

    if not command_found:
        abs_command = ''

    return abs_command


def is_installed(prog):
    """
    See whether "prog" is installed
    """

    wprog = which(prog)

    if wprog == '':
        print prog + ': command not found'
        raise SystemExit
    else:
        if verbose:
            print wprog + ': found'


if __name__ == '__main__':

    import os
    import sys
    import getopt
    import shutil
    import tempfile
    import glob
    import tarfile

    try:
        if sys.version_info[0:3] < (2, 3, 0):
            raise SystemExit
    except:
        print 'You need Python 2.3.0 or greater to run me!'
        raise SystemExit

    try:
        (opts, args) = getopt.getopt(sys.argv[1:], \
                                         'ho:p:d:w:a:qvt:ekc:r:s:b:f:VCD:L:')
    except:
        print "Something's wrong. Try the '-h' flag."
        raise SystemExit

    opts = dict(opts)

    if not opts and not args:
        print usage
        raise SystemExit

    if '-h' in opts:
        print usage + help
        raise SystemExit

    if '-V' in opts:
        print 'avi_encoder.py version ' + version
        raise SystemExit

    if ('-v' in opts) or ('-C' in opts):
        verbose = True
    else:
        verbose = False

    if '-q' in opts:
        quiet = True
        verbose = False
    else:
        quiet = False

    for i in [convert, mencoder, lameenc, sox]:
        is_installed(i)
    if '-C' in opts: raise SystemExit

    img_pre = opts.get('-p', '')

    if img_pre not in ['', 'eimg', 'rimg']:
         print 'Improper image name prefix.'
         raise SystemExit

    temp_dir = ''
    img_dir = opts.get('-d', '.')
    img_dir = os.path.abspath(img_dir)
    if ' ' in img_dir:
        temp_dir = tempfile.mkdtemp('', '.lives-', '/tmp/')
        os.symlink(img_dir, temp_dir + '/img_dir')
        img_dir = temp_dir + '/img_dir'

    if not os.path.isdir(img_dir):
        print 'The image source directory: '  + img_dir + \
              ' does not exist!'
        raise SystemExit

    if len(args) not in [0, 2]:
        print 'If frames are specified both first and last ' + \
              'image numbers must be chosen.'
        raise SystemExit
    elif len(args) == 0:
        args = [None, None]

    frame_range = False
    if not args[0]:
        first_frame_num = '1'
    else:
        frame_range = True
        first_frame_num = args[0]
    first_frame_num = first_frame_num.zfill(8)
    first_num = int(first_frame_num)

    if not args[1]:
        last_frame_num = '99999999'
    else:
        last_frame_num = args[1]
    last_frame_num = last_frame_num.zfill(8)
    last_num = int(last_frame_num)

    aspectc = opts.get('-a', '2')

    if aspectc not in [str(i) for i in xrange(1,5)]:
        print 'Invalid aspect ratio.'
        raise SystemExit
    else:
        if aspectc == '1': aspect = 1.0/1.0
        elif aspectc == '2': aspect = 4.0/3.0
        elif aspectc == '3': aspect = 16.0/9.0
        elif aspectc == '4': aspect = 2.21/1.0

    vtype = opts.get('-t', 'ml')

    out_avi = opts.get('-o', vtype + '_movie.avi')

    if '_' not in vtype:
        vtype = vtype + '_x'

    if vtype not in ['hi_s', 'mh_s', 'ml_s', 'lo_s', \
                     'hi_h', 'mh_h', 'ml_h', 'lo_h', \
                     'hi_x', 'mh_x', 'ml_x', 'lo_x']:
        print 'Invalid video type.'
        raise SystemExit

    fpsc = opts.get('-f', '4')

    if fpsc not in [str(i) for i in xrange(1,9)]:
        if not quiet: print 'Invalid fps code, attempting float fps.'
        foundfps = False
    else:
        if fpsc == '1': fps = 24000.0/1001.0
        elif fpsc == '2': fps = 24.0
        elif fpsc == '3': fps = 25.0
        elif fpsc == '4': fps = 30000.0/1001.0
        elif fpsc == '5': fps = 30.0
        elif fpsc == '6': fps = 50.0
        elif fpsc == '7': fps = 60000.0/1001.0
        elif fpsc == '8': fps = 60.0
        foundfps = True

    if not foundfps:
        try:
            fps = locale.atof(fpsc)
            if not quiet: print 'Using fps = %s' % fps
            if fps > 0: foundfps = True
        except:
            pass

    if not foundfps:
        print 'Invalid fps code or rate.'
        raise SystemExit

    if '-e' not in opts:
        enhance = False
    else:
        enhance = True

    if enhance and img_pre:
        print 'Sorry, you cannot enhance already-enhanced images'
        raise SystemExit

    if '-k' not in opts:
        keep = False
    else:
        keep = True

    cgeom = opts.get('-c', '')
    rgeom = opts.get('-r', '')

    if (cgeom or rgeom) and not enhance:
        print 'Missing "-e" option.'
        raise SystemExit

    lv1file = opts.get('-L', None)
    if lv1file:
        if not quiet: print 'Opening lv1 file...'
        try:
            lv1 = tarfile.open(os.path.abspath(lv1file))
        except:
            print 'This does not appear to be a valid LiVES file!'
            raise SystemExit
        if 'header.tar' not in lv1.getnames():
            print 'This does not appear to be a valid LiVES file!'
            raise SystemExit
        for tfile in lv1.getmembers():
            lv1.extract(tfile, img_dir)
        for tfile in glob.glob(os.path.join(img_dir, '*.tar')):
            imgtar = tarfile.open(tfile)
            for img in imgtar.getmembers():
                imgtar.extract(img, img_dir)
            os.remove(tfile)

    test_file = os.path.join(img_dir, img_pre + first_frame_num)
    if os.path.isfile(test_file + '.jpg'):
        ext = '.jpg'
    elif os.path.isfile(test_file + '.png'):
        ext = '.png'
    else:
        print 'Cannot find any appropriate %s or %s files!' % ('.jpg','.png')
        raise SystemExit
    first_frame = test_file + ext
    last_frame = os.path.join(img_dir, img_pre + last_frame_num + ext)

    if not quiet: print 'Found: ' + first_frame

    work_dir = opts.get('-w', img_dir)
    work_dir = os.path.abspath(work_dir)
    if not os.path.isdir(work_dir):
        if not quiet: print 'Creating ' + work_dir
        try:
            os.makedirs(work_dir)
            os.chmod(work_dir, 0755)
        except:
            print 'Could not create the work directory ' + \
                  work_dir
            raise SystemExit
    if ' ' in work_dir:
        if temp_dir == '':
            temp_dir = tempfile.mkdtemp('', '.lives-', '/tmp/')
        os.symlink(work_dir, temp_dir + '/work_dir')
        work_dir = temp_dir + '/work_dir'

    sndf = opts.get('-s', os.path.join(img_dir, 'audio'))
#    sndf = os.path.abspath(sndf)
    rawsndf = True
    if not os.path.isfile(sndf):
        audio = False
        rawsndf = False
    else:
        audio = True
        if sndf[-4:] == '.wav':
            rawsndf = False
        if not quiet: print 'Found audio file: ' + sndf

    sndr = opts.get('-b', '44100')

    # We use here mencoder's -audio-delay which is not really
    # the true linear delay of the other plugins. See the mencoder
    # man page. Negative values do not work. Must be in seconds.
    delay = opts.get('-D', '0')
    try:
        delay = '%s' % (locale.atof(delay)/1000.0,)
    except:
        print 'Invalid audio delay: ' + delay
    if verbose and audio: print 'Audio delay (ms): ' + delay

    if enhance:
        do_enhance()
        # Note that do_enhance() always creates images prefixed
        # with 'rimg'. What's important to note if that if the
        # images are resized the 'rimg' are indeed resized, but
        # if not the 'rimg' are simply symlinks to the 'eimg'
        # (enhanced) images.
        img_pre = 'rimg'
        ext = '.png'
    vidname = os.path.join(work_dir, out_avi)
    # do_encode() acts on images prefixed by img_pre.
    do_encode()
    if not keep:
        do_clean()
    if temp_dir != '':
        shutil.rmtree(temp_dir)
    if not quiet: print "Done!"


"""
CHANGELOG:

03 Jan 2005 : 0.0.1 : first release, based on mkv_encoder.py.
04 Jan 2005 : 0.0.2 : added support for h.264.
10 Mar 2005 : 0.0.3 : fixed snow quality settings.
26 Mar 2005 : 0.0.4 : use env python (hopefully >= 2.3).
                      replace denoise3d with hqdn3d.
24 Aug 2005 : 0.0.5 : fixed 2.21:1 aspect ratio.
                      fine-tune mplayer filters.
                      expanded h.264 bit rates.
09 Mar 2006 : 0.0.6 : added '-depth 8' to resize, as ImageMagick
                      keeps changing its damn behaviour.
                      Fixed snow encoding (still highly experimental).
                      Fixed bitrate levels.
13 Mar 2006 : 0.0.7 : tweaked bitrate levels.
28 Jun 2007 : 0.0.8 : handles paths with spaces appropriately
                      (thanks Gabriel).
08 Dec 2007 : 0.0.9 : added "-mc 0" and "-noskip" options.
"""
