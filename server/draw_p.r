args <- commandArgs(TRUE)

program <- args[1]
ext <- args[2]

rawdata <- read.table(paste(program, ext, sep=""), skip=1)

#draw footprint curve
saveto <- sprintf("%s.fp.png", program)
x <- rawdata[,1]
y <- rawdata[,2] * 64 # "* 64" converts to bytes
xlab <- "window size (accesses)"
ylab <- "footprint (bytes)"
legend <- c("average footprint")
png(file=saveto, width=600, height=450)
plot(x, y, type="l", lty=1, xlab=xlab, ylab=ylab,
	frame.plot=TRUE, col="red", main="Average Footprint")
dev.off()